// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/url_checker_holder.h"

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
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

UrlCheckerHolder::OnCompleteCheckResult::OnCompleteCheckResult(
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

UrlCheckerHolder::StartParams::StartParams(
    net::HttpRequestHeaders headers,
    int load_flags,
    bool has_user_gesture,
    GURL url,
    std::string method)
    : headers(headers),
      load_flags(load_flags),
      has_user_gesture(has_user_gesture),
      url(url),
      method(method) {}

UrlCheckerHolder::StartParams::StartParams(const StartParams& other) = default;

UrlCheckerHolder::StartParams::~StartParams() = default;

UrlCheckerHolder::UrlCheckerHolder(
    GetDelegateCallback delegate_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id,
    base::RepeatingCallback<content::WebContents*()> web_contents_getter,
    OnCompleteCheckCallback complete_callback,
    bool url_real_time_lookup_enabled,
    bool can_check_db,
    bool can_check_high_confidence_allowlist,
    std::string url_lookup_service_metric_suffix,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
    base::WeakPtr<HashRealTimeService> hash_realtime_service,
    hash_realtime_utils::HashRealTimeSelection hash_realtime_selection,
    bool is_async_check,
    bool check_allowlist_before_hash_database,
    SessionID tab_id)
    : delegate_getter_(std::move(delegate_getter)),
      frame_tree_node_id_(frame_tree_node_id),
      navigation_id_(navigation_id),
      web_contents_getter_(web_contents_getter),
      complete_callback_(std::move(complete_callback)),
      url_real_time_lookup_enabled_(url_real_time_lookup_enabled),
      can_check_db_(can_check_db),
      can_check_high_confidence_allowlist_(can_check_high_confidence_allowlist),
      url_lookup_service_metric_suffix_(url_lookup_service_metric_suffix),
      url_lookup_service_(url_lookup_service),
      hash_realtime_service_(hash_realtime_service),
      hash_realtime_selection_(hash_realtime_selection),
      creation_time_(base::TimeTicks::Now()),
      is_async_check_(is_async_check),
      check_allowlist_before_hash_database_(
          check_allowlist_before_hash_database),
      tab_id_(tab_id) {}

UrlCheckerHolder::~UrlCheckerHolder() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::UmaHistogramMediumTimes(
      "SafeBrowsing.BrowserThrottle.CheckerOnIOLifetime",
      base::TimeTicks::Now() - creation_time_);
}

void UrlCheckerHolder::Start(const StartParams& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate =
      std::move(delegate_getter_).Run();

  if (url_checker_for_testing_) {
    url_checker_ = std::move(url_checker_for_testing_);
  } else {
    url_checker_ = std::make_unique<SafeBrowsingUrlCheckerImpl>(
        params.headers, params.load_flags, params.has_user_gesture,
        url_checker_delegate, web_contents_getter_, nullptr,
        content::ChildProcessHost::kInvalidUniqueID, std::nullopt,
        frame_tree_node_id_.value(), navigation_id_,
        url_real_time_lookup_enabled_, can_check_db_,
        can_check_high_confidence_allowlist_, url_lookup_service_metric_suffix_,
        content::GetUIThreadTaskRunner({}), url_lookup_service_,
        hash_realtime_service_, hash_realtime_selection_, is_async_check_,
        check_allowlist_before_hash_database_, tab_id_);
  }

  CheckUrl(params.url, params.method);
}

void UrlCheckerHolder::CheckUrl(const GURL& url, const std::string& method) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(url_checker_);
  pending_checks_++;
  redirect_chain_.push_back(url);
  url_checker_->CheckUrl(url, method,
                         base::BindOnce(&UrlCheckerHolder::OnCheckUrlResult,
                                        base::Unretained(this)));
}

void UrlCheckerHolder::SwapCompleteCallback(OnCompleteCheckCallback callback) {
  complete_callback_ = std::move(callback);
}

const std::vector<GURL>& UrlCheckerHolder::GetRedirectChain() {
  return redirect_chain_;
}

void UrlCheckerHolder::SetUrlCheckerForTesting(
    std::unique_ptr<SafeBrowsingUrlCheckerImpl> checker) {
  url_checker_for_testing_ = std::move(checker);
}

bool UrlCheckerHolder::IsRealTimeCheckForTesting() {
  return url_real_time_lookup_enabled_ ||
         hash_realtime_selection_ !=
             hash_realtime_utils::HashRealTimeSelection::kNone;
}

bool UrlCheckerHolder::IsAsyncCheckForTesting() {
  return is_async_check_;
}

bool UrlCheckerHolder::IsCheckAllowlistBeforeHashDatabaseForTesting() {
  return check_allowlist_before_hash_database_;
}

void UrlCheckerHolder::AddUrlInRedirectChainForTesting(const GURL& url) {
  redirect_chain_.push_back(url);
}

void UrlCheckerHolder::OnCheckUrlResult(
    bool proceed,
    bool showed_interstitial,
    bool has_post_commit_interstitial_skipped,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  pending_checks_--;
  bool all_checks_completed = pending_checks_ == 0;
  OnCompleteCheckResult result(proceed, showed_interstitial,
                               has_post_commit_interstitial_skipped,
                               performed_check, all_checks_completed);
  complete_callback_.Run(result);
}

}  // namespace safe_browsing
