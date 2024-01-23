// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/url_checker_on_sb.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

UrlCheckerOnSB::OnCompleteCheckResult::OnCompleteCheckResult(
    bool proceed,
    bool showed_interstitial,
    bool has_post_commit_interstitial_skipped,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check,
    bool all_checks_completed)
    : proceed(proceed),
      showed_interstitial(showed_interstitial),
      has_post_commit_interstitial_skipped(
          has_post_commit_interstitial_skipped),
      performed_check(performed_check),
      all_checks_completed(all_checks_completed) {}

UrlCheckerOnSB::StartParams::StartParams(
    net::HttpRequestHeaders headers,
    int load_flags,
    network::mojom::RequestDestination request_destination,
    bool has_user_gesture,
    GURL url,
    std::string method)
    : headers(headers),
      load_flags(load_flags),
      request_destination(request_destination),
      has_user_gesture(has_user_gesture),
      url(url),
      method(method) {}

UrlCheckerOnSB::StartParams::StartParams(const StartParams& other) = default;

UrlCheckerOnSB::StartParams::~StartParams() = default;

UrlCheckerOnSB::UrlCheckerOnSB(
    GetDelegateCallback delegate_getter,
    int frame_tree_node_id,
    absl::optional<int64_t> navigation_id,
    base::RepeatingCallback<content::WebContents*()> web_contents_getter,
    OnCompleteCheckCallback complete_callback,
    bool url_real_time_lookup_enabled,
    bool can_urt_check_subresource_url,
    bool can_check_db,
    bool can_check_high_confidence_allowlist,
    std::string url_lookup_service_metric_suffix,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
    base::WeakPtr<HashRealTimeService> hash_realtime_service,
    hash_realtime_utils::HashRealTimeSelection hash_realtime_selection)
    : delegate_getter_(std::move(delegate_getter)),
      frame_tree_node_id_(frame_tree_node_id),
      navigation_id_(navigation_id),
      web_contents_getter_(web_contents_getter),
      complete_callback_(std::move(complete_callback)),
      url_real_time_lookup_enabled_(url_real_time_lookup_enabled),
      can_urt_check_subresource_url_(can_urt_check_subresource_url),
      can_check_db_(can_check_db),
      can_check_high_confidence_allowlist_(can_check_high_confidence_allowlist),
      url_lookup_service_metric_suffix_(url_lookup_service_metric_suffix),
      url_lookup_service_(url_lookup_service),
      hash_realtime_service_(hash_realtime_service),
      hash_realtime_selection_(hash_realtime_selection),
      creation_time_(base::TimeTicks::Now()) {
  content::WebContents* contents = web_contents_getter_.Run();
  if (!!contents) {
    last_committed_url_ = contents->GetLastCommittedURL();
  }
}

UrlCheckerOnSB::~UrlCheckerOnSB() {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);
  base::UmaHistogramMediumTimes(
      "SafeBrowsing.BrowserThrottle.CheckerOnIOLifetime",
      base::TimeTicks::Now() - creation_time_);
}

void UrlCheckerOnSB::Start(const StartParams& params) {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate =
      std::move(delegate_getter_).Run();

  if (url_checker_for_testing_) {
    url_checker_ = std::move(url_checker_for_testing_);
  } else {
    url_checker_ = std::make_unique<SafeBrowsingUrlCheckerImpl>(
        params.headers, params.load_flags, params.request_destination,
        params.has_user_gesture, url_checker_delegate, web_contents_getter_,
        nullptr, content::ChildProcessHost::kInvalidUniqueID, std::nullopt,
        frame_tree_node_id_, navigation_id_, url_real_time_lookup_enabled_,
        can_urt_check_subresource_url_, can_check_db_,
        can_check_high_confidence_allowlist_, url_lookup_service_metric_suffix_,
        last_committed_url_, content::GetUIThreadTaskRunner({}),
        url_lookup_service_, hash_realtime_service_, hash_realtime_selection_);
  }

  CheckUrl(params.url, params.method);
}

void UrlCheckerOnSB::CheckUrl(const GURL& url, const std::string& method) {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);
  DCHECK(url_checker_);
  pending_checks_++;
  redirect_chain_.push_back(url);
  url_checker_->CheckUrl(url, method,
                         base::BindOnce(&UrlCheckerOnSB::OnCheckUrlResult,
                                        base::Unretained(this)));
}

void UrlCheckerOnSB::SwapCompleteCallback(OnCompleteCheckCallback callback) {
  complete_callback_ = std::move(callback);
}

const std::vector<GURL>& UrlCheckerOnSB::GetRedirectChain() {
  return redirect_chain_;
}

void UrlCheckerOnSB::SetUrlCheckerForTesting(
    std::unique_ptr<SafeBrowsingUrlCheckerImpl> checker) {
  url_checker_for_testing_ = std::move(checker);
}

bool UrlCheckerOnSB::IsRealTimeCheckForTesting() {
  return url_real_time_lookup_enabled_ ||
         hash_realtime_selection_ !=
             hash_realtime_utils::HashRealTimeSelection::kNone;
}

void UrlCheckerOnSB::AddUrlInRedirectChainForTesting(const GURL& url) {
  redirect_chain_.push_back(url);
}

void UrlCheckerOnSB::OnCheckUrlResult(
    NativeUrlCheckNotifier* slow_check_notifier,
    bool proceed,
    bool showed_interstitial,
    bool has_post_commit_interstitial_skipped,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  pending_checks_--;
  OnCompleteCheck(proceed, showed_interstitial,
                  has_post_commit_interstitial_skipped, performed_check);
}

void UrlCheckerOnSB::OnCompleteCheck(
    bool proceed,
    bool showed_interstitial,
    bool has_post_commit_interstitial_skipped,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  bool all_checks_completed = pending_checks_ == 0;
  OnCompleteCheckResult result(proceed, showed_interstitial,
                               has_post_commit_interstitial_skipped,
                               performed_check, all_checks_completed);
  if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
    complete_callback_.Run(result);
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(complete_callback_, result));
  }
}

}  // namespace safe_browsing
