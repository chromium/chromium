// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/browser/safe_browsing_url_checker_impl.h"

#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/browser/url_checker_delegate.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/web_ui/constants.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log_event_type.h"

namespace safe_browsing {
namespace {

// Maximum time in milliseconds to wait for the SafeBrowsing service reputation
// check. After this amount of time the outstanding check will be aborted, and
// the resource will be treated as if it were safe.
const int kCheckUrlTimeoutMs = 5000;

}  // namespace

SafeBrowsingUrlCheckerImpl::Notifier::Notifier(CheckUrlCallback callback)
    : callback_(std::move(callback)) {}

SafeBrowsingUrlCheckerImpl::Notifier::Notifier(
    NativeCheckUrlCallback native_callback)
    : native_callback_(std::move(native_callback)) {}

SafeBrowsingUrlCheckerImpl::Notifier::~Notifier() = default;

SafeBrowsingUrlCheckerImpl::Notifier::Notifier(Notifier&& other) = default;

SafeBrowsingUrlCheckerImpl::Notifier& SafeBrowsingUrlCheckerImpl::Notifier::
operator=(Notifier&& other) = default;

void SafeBrowsingUrlCheckerImpl::Notifier::OnStartSlowCheck() {
  if (callback_) {
    std::move(callback_).Run(mojo::MakeRequest(&slow_check_notifier_), false,
                             false);
    return;
  }

  DCHECK(native_callback_);
  std::move(native_callback_).Run(&native_slow_check_notifier_, false, false);
}

void SafeBrowsingUrlCheckerImpl::Notifier::OnCompleteCheck(
    bool proceed,
    bool showed_interstitial) {
  if (callback_) {
    std::move(callback_).Run(nullptr, proceed, showed_interstitial);
    return;
  }

  if (native_callback_) {
    std::move(native_callback_).Run(nullptr, proceed, showed_interstitial);
    return;
  }

  if (slow_check_notifier_) {
    slow_check_notifier_->OnCompleteCheck(proceed, showed_interstitial);
    slow_check_notifier_.reset();
    return;
  }

  std::move(native_slow_check_notifier_).Run(proceed, showed_interstitial);
}

SafeBrowsingUrlCheckerImpl::UrlInfo::UrlInfo(const GURL& in_url,
                                             const std::string& in_method,
                                             Notifier in_notifier)
    : url(in_url), method(in_method), notifier(std::move(in_notifier)) {}

SafeBrowsingUrlCheckerImpl::UrlInfo::UrlInfo(UrlInfo&& other) = default;

SafeBrowsingUrlCheckerImpl::UrlInfo::~UrlInfo() = default;

SafeBrowsingUrlCheckerImpl::SafeBrowsingUrlCheckerImpl(
    const net::HttpRequestHeaders& headers,
    int load_flags,
    content::ResourceType resource_type,
    bool has_user_gesture,
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
    const base::Callback<content::WebContents*()>& web_contents_getter)
    : headers_(headers),
      load_flags_(load_flags),
      resource_type_(resource_type),
      has_user_gesture_(has_user_gesture),
      web_contents_getter_(web_contents_getter),
      url_checker_delegate_(std::move(url_checker_delegate)),
      database_manager_(url_checker_delegate_->GetDatabaseManager()),
      weak_factory_(this) {}

SafeBrowsingUrlCheckerImpl::~SafeBrowsingUrlCheckerImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == STATE_CHECKING_URL) {
    database_manager_->CancelCheck(this);

    TRACE_EVENT_ASYNC_END1("safe_browsing", "CheckUrl", this, "result",
                           "request_canceled");
  }
}

void SafeBrowsingUrlCheckerImpl::CheckUrl(const GURL& url,
                                          const std::string& method,
                                          CheckUrlCallback callback) {
  CheckUrlImpl(url, method, Notifier(std::move(callback)));
}

void SafeBrowsingUrlCheckerImpl::CheckUrl(const GURL& url,
                                          const std::string& method,
                                          NativeCheckUrlCallback callback) {
  CheckUrlImpl(url, method, Notifier(std::move(callback)));
}

void SafeBrowsingUrlCheckerImpl::OnCheckBrowseUrlResult(
    const GURL& url,
    SBThreatType threat_type,
    const ThreatMetadata& metadata) {
  DCHECK_EQ(STATE_CHECKING_URL, state_);
  DCHECK_LT(next_index_, urls_.size());
  DCHECK_EQ(urls_[next_index_].url, url);

  timer_.Stop();

  TRACE_EVENT_ASYNC_END1(
      "safe_browsing", "CheckUrl", this, "result",
      threat_type == SB_THREAT_TYPE_SAFE ? "safe" : "unsafe");

  if (threat_type == SB_THREAT_TYPE_SAFE ||
      threat_type == SB_THREAT_TYPE_SUSPICIOUS_SITE ||
      (!base::FeatureList::IsEnabled(safe_browsing::kBillingInterstitial) &&
       threat_type == SB_THREAT_TYPE_BILLING)) {
    state_ = STATE_NONE;

    if (threat_type == SB_THREAT_TYPE_SUSPICIOUS_SITE) {
      url_checker_delegate_->NotifySuspiciousSiteDetected(web_contents_getter_);
    }

    if (!RunNextCallback(true, false))
      return;

    ProcessUrls();
    return;
  }

  if (load_flags_ & net::LOAD_PREFETCH) {
    // Destroy the prefetch with FINAL_STATUS_SAFEBROSWING.
    if (resource_type_ == content::RESOURCE_TYPE_MAIN_FRAME) {
      url_checker_delegate_->MaybeDestroyPrerenderContents(
          web_contents_getter_);
    }
    UMA_HISTOGRAM_ENUMERATION("SB2.ResourceTypes2.UnsafePrefetchCanceled",
                              resource_type_, content::RESOURCE_TYPE_LAST_TYPE);
    BlockAndProcessUrls(false);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("SB2.ResourceTypes2.Unsafe", resource_type_,
                            content::RESOURCE_TYPE_LAST_TYPE);

  security_interstitials::UnsafeResource resource;
  resource.url = url;
  resource.original_url = urls_[0].url;
  if (urls_.size() > 1) {
    resource.redirect_urls.reserve(urls_.size() - 1);
    for (size_t i = 1; i < urls_.size(); ++i)
      resource.redirect_urls.push_back(urls_[i].url);
  }
  resource.is_subresource = resource_type_ != content::RESOURCE_TYPE_MAIN_FRAME;
  resource.is_subframe = resource_type_ == content::RESOURCE_TYPE_SUB_FRAME;
  resource.threat_type = threat_type;
  resource.threat_metadata = metadata;
  resource.callback =
      base::Bind(&SafeBrowsingUrlCheckerImpl::OnBlockingPageComplete,
                 weak_factory_.GetWeakPtr());
  resource.callback_thread = base::CreateSingleThreadTaskRunnerWithTraits(
      {content::BrowserThread::IO});
  resource.web_contents_getter = web_contents_getter_;
  resource.threat_source = database_manager_->GetThreatSource();

  state_ = STATE_DISPLAYING_BLOCKING_PAGE;
  url_checker_delegate_->StartDisplayingBlockingPageHelper(
      resource, urls_[next_index_].method, headers_,
      resource_type_ == content::RESOURCE_TYPE_MAIN_FRAME, has_user_gesture_);
}

void SafeBrowsingUrlCheckerImpl::OnCheckUrlTimeout() {
  database_manager_->CancelCheck(this);

  OnCheckBrowseUrlResult(urls_[next_index_].url,
                         safe_browsing::SB_THREAT_TYPE_SAFE, ThreatMetadata());
}

void SafeBrowsingUrlCheckerImpl::CheckUrlImpl(const GURL& url,
                                              const std::string& method,
                                              Notifier notifier) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  DVLOG(1) << "SafeBrowsingUrlCheckerImpl checks URL: " << url;
  urls_.emplace_back(url, method, std::move(notifier));

  ProcessUrls();
}

void SafeBrowsingUrlCheckerImpl::ProcessUrls() {
  DCHECK_NE(STATE_BLOCKED, state_);

  if (state_ == STATE_CHECKING_URL ||
      state_ == STATE_DISPLAYING_BLOCKING_PAGE) {
    return;
  }

  while (next_index_ < urls_.size()) {
    DCHECK_EQ(STATE_NONE, state_);

    const GURL& url = urls_[next_index_].url;

    if (url_checker_delegate_->IsUrlWhitelisted(url)) {
      if (!RunNextCallback(true, false))
        return;

      continue;
    }

    // TODO(yzshen): Consider moving CanCheckResourceType() to the renderer
    // side. That would save some IPCs. It requires a method on the
    // SafeBrowsing mojo interface to query all supported resource types.
    if (!database_manager_->CanCheckResourceType(resource_type_)) {
      // TODO(vakh): Consider changing this metric to
      // SafeBrowsing.V4ResourceType to be consistent with the other PVer4
      // metrics.
      UMA_HISTOGRAM_ENUMERATION("SB2.ResourceTypes2.Skipped", resource_type_,
                                content::RESOURCE_TYPE_LAST_TYPE);

      if (!RunNextCallback(true, false))
        return;

      continue;
    }

    // TODO(vakh): Consider changing this metric to SafeBrowsing.V4ResourceType
    // to be consistent with the other PVer4 metrics.
    UMA_HISTOGRAM_ENUMERATION("SB2.ResourceTypes2.Checked", resource_type_,
                              content::RESOURCE_TYPE_LAST_TYPE);

    SBThreatType threat_type = CheckWebUIUrls(url);
    if (threat_type != safe_browsing::SB_THREAT_TYPE_SAFE) {
      state_ = STATE_CHECKING_URL;
      TRACE_EVENT_ASYNC_BEGIN1("safe_browsing", "CheckUrl", this, "url",
                               url.spec());

      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::IO},
          base::BindOnce(&SafeBrowsingUrlCheckerImpl::OnCheckBrowseUrlResult,
                         weak_factory_.GetWeakPtr(), url, threat_type,
                         ThreatMetadata()));
      break;
    }

    if (database_manager_->CheckBrowseUrl(
            url, url_checker_delegate_->GetThreatTypes(), this)) {
      if (!RunNextCallback(true, false))
        return;

      continue;
    }

    state_ = STATE_CHECKING_URL;

    // Only send out notification of starting a slow check if the database
    // manager actually supports fast checks (i.e., synchronous checks) but is
    // not able to complete the check synchronously in this case.
    // Don't send out notification if the database manager doesn't support
    // synchronous checks at all (e.g., on mobile).
    if (!database_manager_->ChecksAreAlwaysAsync())
      urls_[next_index_].notifier.OnStartSlowCheck();

    TRACE_EVENT_ASYNC_BEGIN1("safe_browsing", "CheckUrl", this, "url",
                             url.spec());

    // Start a timer to abort the check if it takes too long.
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kCheckUrlTimeoutMs), this,
                 &SafeBrowsingUrlCheckerImpl::OnCheckUrlTimeout);

    break;
  }
}

void SafeBrowsingUrlCheckerImpl::BlockAndProcessUrls(bool showed_interstitial) {
  DVLOG(1) << "SafeBrowsingUrlCheckerImpl blocks URL: "
           << urls_[next_index_].url;
  state_ = STATE_BLOCKED;

  // If user decided to not proceed through a warning, mark all the remaining
  // redirects as "bad".
  while (next_index_ < urls_.size()) {
    if (!RunNextCallback(false, showed_interstitial))
      return;
  }
}

void SafeBrowsingUrlCheckerImpl::OnBlockingPageComplete(bool proceed) {
  DCHECK_EQ(STATE_DISPLAYING_BLOCKING_PAGE, state_);

  if (proceed) {
    state_ = STATE_NONE;
    if (!RunNextCallback(true, true))
      return;
    ProcessUrls();
  } else {
    BlockAndProcessUrls(true);
  }
}

SBThreatType SafeBrowsingUrlCheckerImpl::CheckWebUIUrls(const GURL& url) {
  if (url == kChromeUISafeBrowsingMatchMalwareUrl)
    return safe_browsing::SB_THREAT_TYPE_URL_MALWARE;
  if (url == kChromeUISafeBrowsingMatchPhishingUrl)
    return safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  if (url == kChromeUISafeBrowsingMatchUnwantedUrl)
    return safe_browsing::SB_THREAT_TYPE_URL_UNWANTED;
  if (url == kChromeUISafeBrowsingMatchBillingUrl)
    return safe_browsing::SB_THREAT_TYPE_BILLING;

  return safe_browsing::SB_THREAT_TYPE_SAFE;
}

bool SafeBrowsingUrlCheckerImpl::RunNextCallback(bool proceed,
                                                 bool showed_interstitial) {
  DCHECK_LT(next_index_, urls_.size());

  auto weak_self = weak_factory_.GetWeakPtr();
  urls_[next_index_++].notifier.OnCompleteCheck(proceed, showed_interstitial);
  return !!weak_self;
}

}  // namespace safe_browsing
