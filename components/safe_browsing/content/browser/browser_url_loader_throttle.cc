// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr char kFullURLLookup[] = "FullUrlLookup";

constexpr char kFromCacheUmaSuffix[] = ".FromCache";
constexpr char kFromNetworkUmaSuffix[] = ".FromNetwork";

void LogTotalDelay2Metrics(const std::string& url_check_type,
                           base::TimeDelta total_delay) {
  base::UmaHistogramTimes(
      base::StrCat(
          {"SafeBrowsing.BrowserThrottle.TotalDelay2", url_check_type}),
      total_delay);
}

void LogTotalDelay2MetricsWithResponseType(bool is_response_from_cache,
                                           base::TimeDelta total_delay) {
  base::UmaHistogramTimes(
      base::StrCat({"SafeBrowsing.BrowserThrottle.TotalDelay2",
                    is_response_from_cache ? kFromCacheUmaSuffix
                                           : kFromNetworkUmaSuffix}),
      total_delay);
}

// Returns true if the URL is known to be safe. We also require that this URL
// never redirects to a potentially unsafe URL, because the redirected URLs are
// also skipped if this function returns true.
bool KnownSafeUrl(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         !safe_browsing::IsSafeBrowsingWebUIUrl(url);
}

}  // namespace

namespace safe_browsing {

BrowserURLLoaderThrottle::SkipCheckChecker::SkipCheckChecker(
    UrlCheckerHolder::GetDelegateCallback delegate_getter,
    content::FrameTreeNodeId frame_tree_node_id)
    : delegate_getter_(std::move(delegate_getter)),
      frame_tree_node_id_(frame_tree_node_id) {}

BrowserURLLoaderThrottle::SkipCheckChecker::~SkipCheckChecker() = default;

void BrowserURLLoaderThrottle::SkipCheckChecker::CheckOriginalUrl(
    OnCompleteCheckCallback callback,
    const GURL& url,
    bool originated_from_service_worker) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate =
      std::move(delegate_getter_).Run();
  should_skip_checks_ =
      !url_checker_delegate ||
      url_checker_delegate->ShouldSkipRequestCheck(
          url, frame_tree_node_id_.value(),
          /*render_process_id=*/content::ChildProcessHost::kInvalidUniqueID,
          /*render_frame_token=*/std::nullopt, originated_from_service_worker);
  std::move(callback).Run(should_skip_checks_);
}

void BrowserURLLoaderThrottle::SkipCheckChecker::CheckRedirectUrl(
    OnCompleteCheckCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(should_skip_checks_);
}

// static
std::unique_ptr<BrowserURLLoaderThrottle> BrowserURLLoaderThrottle::Create(
    UrlCheckerHolder::GetDelegateCallback delegate_getter,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
    base::WeakPtr<HashRealTimeService> hash_realtime_service,
    hash_realtime_utils::HashRealTimeSelection hash_realtime_selection,
    base::WeakPtr<AsyncCheckTracker> async_check_tracker) {
  return base::WrapUnique<BrowserURLLoaderThrottle>(
      new BrowserURLLoaderThrottle(
          std::move(delegate_getter), web_contents_getter, frame_tree_node_id,
          navigation_id, url_lookup_service, hash_realtime_service,
          hash_realtime_selection, async_check_tracker));
}

BrowserURLLoaderThrottle::BrowserURLLoaderThrottle(
    UrlCheckerHolder::GetDelegateCallback delegate_getter,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
    base::WeakPtr<HashRealTimeService> hash_realtime_service,
    hash_realtime_utils::HashRealTimeSelection hash_realtime_selection,
    base::WeakPtr<AsyncCheckTracker> async_check_tracker)
    : async_check_tracker_(async_check_tracker),
      url_lookup_service_(url_lookup_service),
      hash_realtime_service_(hash_realtime_service),
      hash_realtime_selection_(hash_realtime_selection),
      frame_tree_node_id_(frame_tree_node_id),
      navigation_id_(navigation_id),
      delegate_getter_(delegate_getter),
      web_contents_getter_(web_contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Decide whether to do real time URL lookups or not.
  url_real_time_lookup_enabled_ =
      url_lookup_service_ ? url_lookup_service_->CanPerformFullURLLookup()
                          : false;
  url_lookup_service_metric_suffix_ =
      url_real_time_lookup_enabled_ ? url_lookup_service_->GetMetricSuffix()
                                    : kNoRealTimeURLLookupService;
  content::WebContents* web_contents = web_contents_getter_.Run();
  tab_id_ = web_contents ? sessions::SessionTabHelper::IdForTab(web_contents)
                         : SessionID::InvalidValue();
  skip_check_checker_ =
      std::make_unique<SkipCheckChecker>(delegate_getter_, frame_tree_node_id);
}

BrowserURLLoaderThrottle::~BrowserURLLoaderThrottle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (deferred_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                    TRACE_ID_LOCAL(this));
  }
  if (was_async_faster_than_sync_.has_value()) {
    base::UmaHistogramBoolean(
        "SafeBrowsing.BrowserThrottle.IsAsyncCheckFasterThanSyncCheck",
        was_async_faster_than_sync_.value());
  }
  DeleteUrlChecker();
}

void BrowserURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(0u, pending_sync_checks_);
  DCHECK_EQ(0u, pending_async_checks_);
  DCHECK(!blocked_);

  base::UmaHistogramEnumeration(
      "SafeBrowsing.BrowserThrottle.RequestDestination", request->destination);

  if (KnownSafeUrl(request->url)) {
    skip_checks_ = true;
    return;
  }

  if (request->destination != network::mojom::RequestDestination::kDocument) {
    VLOG(2) << __func__ << " : Skipping: " << request->url << " : "
            << request->destination;
    base::UmaHistogramEnumeration(
        "SafeBrowsing.BrowserThrottle.RequestDestination.Skipped",
        request->destination);
    skip_checks_ = true;
    return;
  }

  base::UmaHistogramEnumeration(
      "SafeBrowsing.BrowserThrottle.RequestDestination.Checked",
      request->destination);

  // Decide whether safe browsing database can be checked.
  // If url_lookup_service_ is null, safe browsing database should be checked by
  // default.
  bool can_check_db = url_lookup_service_
                          ? url_lookup_service_->CanCheckSafeBrowsingDb()
                          : true;
  bool can_check_high_confidence_allowlist =
      url_lookup_service_
          ? url_lookup_service_->CanCheckSafeBrowsingHighConfidenceAllowlist()
          : true;

  if (async_check_tracker_ && navigation_id_.has_value() &&
      !(request->load_flags & net::LOAD_PREFETCH)) {
    CHECK(can_check_db);
    CHECK(url_real_time_lookup_enabled_ ||
          hash_realtime_selection_ !=
              hash_realtime_utils::HashRealTimeSelection::kNone);
    // If async check is enabled, sync_sb_checker only performs local database
    // check.
    sync_sb_checker_ = std::make_unique<UrlCheckerHolder>(
        delegate_getter_, frame_tree_node_id_, navigation_id_,
        web_contents_getter_,
        /*complete_callback=*/
        base::BindRepeating(&BrowserURLLoaderThrottle::OnCompleteSyncCheck,
                            weak_factory_.GetWeakPtr()),
        /*url_real_time_lookup_enabled=*/false, can_check_db,
        /*can_check_high_confidence_allowlist=*/true,
        /*url_lookup_service_metric_suffix=*/kNoRealTimeURLLookupService,
        /*url_lookup_service=*/nullptr,
        /*hash_realtime_service=*/nullptr,
        /*hash_realtime_selection=*/
        hash_realtime_utils::HashRealTimeSelection::kNone,
        /*is_async_check=*/false,
        /*check_allowlist_before_hash_database=*/
        async_check_tracker_->should_sync_checker_check_allowlist(),
        SessionID::InvalidValue());
    async_sb_checker_ = std::make_unique<UrlCheckerHolder>(
        delegate_getter_, frame_tree_node_id_, navigation_id_,
        web_contents_getter_,
        /*complete_callback=*/
        base::BindRepeating(&BrowserURLLoaderThrottle::OnCompleteAsyncCheck,
                            weak_factory_.GetWeakPtr()),
        url_real_time_lookup_enabled_, can_check_db,
        can_check_high_confidence_allowlist, url_lookup_service_metric_suffix_,
        url_lookup_service_, hash_realtime_service_, hash_realtime_selection_,
        /*is_async_check=*/true, /*check_allowlist_before_hash_database=*/false,
        tab_id_);
    if (on_sync_sb_checker_created_callback_for_testing_) {
      std::move(on_sync_sb_checker_created_callback_for_testing_).Run();
    }
    if (on_async_sb_checker_created_callback_for_testing_) {
      std::move(on_async_sb_checker_created_callback_for_testing_).Run();
    }
  } else {
    sync_sb_checker_ = std::make_unique<UrlCheckerHolder>(
        delegate_getter_, frame_tree_node_id_, navigation_id_,
        web_contents_getter_,
        /*complete_callback=*/
        base::BindRepeating(&BrowserURLLoaderThrottle::OnCompleteSyncCheck,
                            weak_factory_.GetWeakPtr()),
        url_real_time_lookup_enabled_, can_check_db,
        can_check_high_confidence_allowlist, url_lookup_service_metric_suffix_,
        url_lookup_service_, hash_realtime_service_, hash_realtime_selection_,
        /*is_async_check=*/false,
        /*check_allowlist_before_hash_database=*/false, tab_id_);
    if (on_sync_sb_checker_created_callback_for_testing_) {
      std::move(on_sync_sb_checker_created_callback_for_testing_).Run();
    }
  }

  pending_sync_checks_++;
  if (async_sb_checker_) {
    pending_async_checks_++;
  }
  was_async_faster_than_sync_ = std::nullopt;
  start_request_time_ = base::TimeTicks::Now();
  is_start_request_called_ = true;

  skip_check_checker_->CheckOriginalUrl(
      base::BindOnce(
          &BrowserURLLoaderThrottle::OnSkipCheckCompleteOnOriginalUrl,
          weak_factory_.GetWeakPtr(), request->headers, request->load_flags,
          request->has_user_gesture, request->url, request->method),
      request->url, request->originated_from_service_worker);
}

void BrowserURLLoaderThrottle::OnSkipCheckCompleteOnOriginalUrl(
    const net::HttpRequestHeaders& headers,
    int load_flags,
    bool has_user_gesture,
    const GURL& url,
    const std::string& method,
    bool should_skip) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (should_skip) {
    SkipChecks();
    return;
  }

  UrlCheckerHolder::StartParams params(headers, load_flags, has_user_gesture,
                                       url, method);
  sync_sb_checker_->Start(params);
  if (async_sb_checker_) {
    async_sb_checker_->Start(params);
  }
}

void BrowserURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& /* response_head */,
    bool* defer,
    std::vector<std::string>* /* to_be_removed_headers */,
    net::HttpRequestHeaders* /* modified_headers */,
    net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (blocked_) {
    // OnCheckUrlResult() has set |blocked_| to true and called
    // |delegate_->CancelWithError|, but this method is called before the
    // request is actually cancelled. In that case, simply defer the request.
    *defer = true;
    return;
  }

  if (skip_checks_) {
    return;
  }

  pending_sync_checks_++;
  if (async_sb_checker_) {
    pending_async_checks_++;
  }
  was_async_faster_than_sync_ = std::nullopt;

  // The check to |skip_check_checker| cannot be skipped because
  // WillRedirectRequest may be called while |skip_check_checker| is still in
  // progress.
  skip_check_checker_->CheckRedirectUrl(base::BindOnce(
      &BrowserURLLoaderThrottle::OnSkipCheckCompleteOnRedirectUrl,
      weak_factory_.GetWeakPtr(), redirect_info->new_url,
      redirect_info->new_method));
}

void BrowserURLLoaderThrottle::OnSkipCheckCompleteOnRedirectUrl(
    const GURL& url,
    const std::string& method,
    bool should_skip) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (should_skip) {
    SkipChecks();
    return;
  }

  if (blocked_) {
    // The original URL may already be blocked while skip check checker is
    // performing the check. In that case, the SB checkers were already deleted.
    return;
  }

  sync_sb_checker_->CheckUrl(url, method);
  if (async_sb_checker_) {
    async_sb_checker_->CheckUrl(url, method);
  }
}

void BrowserURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  will_process_response_count_++;
  base::UmaHistogramCounts100(
      "SafeBrowsing.BrowserThrottle.WillProcessResponseCount",
      will_process_response_count_);

  if (blocked_) {
    // OnCompleteCheck() has set |blocked_| to true and called
    // |delegate_->CancelWithError|, but this method is called before the
    // request is actually cancelled. In that case, simply defer the request.
    *defer = true;
    return;
  }

  bool sync_check_completed = (pending_sync_checks_ == 0);
  base::UmaHistogramBoolean(
      "SafeBrowsing.BrowserThrottle.IsCheckCompletedOnProcessResponse",
      sync_check_completed);
  is_response_from_cache_ =
      response_head->was_fetched_via_cache && !response_head->network_accessed;
  if (is_start_request_called_) {
    base::TimeDelta interval = base::TimeTicks::Now() - start_request_time_;
    base::UmaHistogramTimes(
        "SafeBrowsing.BrowserThrottle.IntervalBetweenStartAndProcess",
        interval);
    base::UmaHistogramTimes(
        base::StrCat(
            {"SafeBrowsing.BrowserThrottle.IntervalBetweenStartAndProcess",
             is_response_from_cache_ ? kFromCacheUmaSuffix
                                     : kFromNetworkUmaSuffix}),
        interval);
    if (sync_check_completed) {
      LogTotalDelay2MetricsWithResponseType(is_response_from_cache_,
                                            base::TimeDelta());
    }
    is_start_request_called_ = false;
  }

  if (sync_check_completed) {
    MaybeTransferAsyncChecker();
    return;
  }

  DCHECK(!deferred_);
  deferred_ = true;
  defer_start_time_ = base::TimeTicks::Now();
  *defer = true;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("safe_browsing", "Deferred",
                                    TRACE_ID_LOCAL(this));
}

const char* BrowserURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "SafeBrowsingBrowserThrottle";
}

UrlCheckerHolder* BrowserURLLoaderThrottle::GetSyncSBCheckerForTesting() {
  return sync_sb_checker_.get();
}

UrlCheckerHolder* BrowserURLLoaderThrottle::GetAsyncSBCheckerForTesting() {
  return async_sb_checker_.get();
}

void BrowserURLLoaderThrottle::SetOnSyncSBCheckerCreatedCallbackForTesting(
    base::OnceClosure callback) {
  on_sync_sb_checker_created_callback_for_testing_ = std::move(callback);
}

void BrowserURLLoaderThrottle::SetOnAsyncSBCheckerCreatedCallbackForTesting(
    base::OnceClosure callback) {
  on_async_sb_checker_created_callback_for_testing_ = std::move(callback);
}

void BrowserURLLoaderThrottle::OnCompleteSyncCheck(
    UrlCheckerHolder::OnCompleteCheckResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(url_real_time_lookup_enabled_ ||
         result.performed_check !=
             SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck);

  // |blocked| may already be set by |async_sb_checker_|.
  if (blocked_) {
    return;
  }

  DCHECK_LT(0u, pending_sync_checks_);
  pending_sync_checks_--;
  if (async_sb_checker_ && pending_sync_checks_ == 0 &&
      !was_async_faster_than_sync_.has_value()) {
    was_async_faster_than_sync_ = false;
  }

  // If the resource load is going to finish (either being cancelled or
  // resumed), record the total delay.
  if (!result.proceed || pending_sync_checks_ == 0) {
    // If the resource load is currently deferred, there is a delay.
    if (deferred_) {
      total_delay_ = base::TimeTicks::Now() - defer_start_time_;
      LogTotalDelay2MetricsWithResponseType(is_response_from_cache_,
                                            total_delay_);
    }
    LogTotalDelay2Metrics(GetUrlCheckTypeForLogging(result.performed_check),
                          total_delay_);
  }

  if (result.proceed) {
    if (pending_sync_checks_ == 0 && deferred_) {
      deferred_ = false;
      TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                      TRACE_ID_LOCAL(this));
      delegate_->Resume();
      MaybeTransferAsyncChecker();
    }
  } else {
    BlockUrlLoader(result.showed_interstitial);
  }
}

void BrowserURLLoaderThrottle::OnCompleteAsyncCheck(
    UrlCheckerHolder::OnCompleteCheckResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |blocked| may already be set by |sync_sb_checker_|.
  if (blocked_) {
    return;
  }

  DCHECK_LT(0u, pending_async_checks_);
  pending_async_checks_--;
  if (pending_async_checks_ == 0 && !was_async_faster_than_sync_.has_value()) {
    was_async_faster_than_sync_ = true;
  }

  if (!result.proceed) {
    BlockUrlLoader(result.showed_interstitial);
  }
  // There is no need to set |deferred_| for async check because it never defers
  // URL loader.
}

void BrowserURLLoaderThrottle::BlockUrlLoader(bool showed_interstitial) {
  blocked_ = true;

  DeleteUrlChecker();
  // If we didn't show an interstitial, we cancel with ERR_ABORTED to not show
  // an error page either.
  delegate_->CancelWithError(
      showed_interstitial ? kNetErrorCodeForSafeBrowsing : net::ERR_ABORTED,
      kCustomCancelReasonForURLLoader);
}

std::string BrowserURLLoaderThrottle::GetUrlCheckTypeForLogging(
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  switch (performed_check) {
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck:
      return base::StrCat({url_lookup_service_metric_suffix_, kFullURLLookup});
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck:
      return ".HashPrefixDatabaseCheck";
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kCheckSkipped:
      return ".SkippedCheck";
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashRealTimeCheck:
      return ".HashPrefixRealTimeCheck";
    case SafeBrowsingUrlCheckerImpl::PerformedCheck::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return ".HashPrefixDatabaseCheck";
  }
}

void BrowserURLLoaderThrottle::SkipChecks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Future checks for redirects will be skipped.
  skip_checks_ = true;

  pending_sync_checks_--;
  if (async_sb_checker_) {
    pending_async_checks_--;
    // Don't set |was_async_faster_than_sync_| if the counter reaches 0,
    // because the checks being skipped is considered a tie between sync and
    // async checks.
  }
  if (pending_sync_checks_ == 0 && deferred_) {
    delegate_->Resume();
  }
}

void BrowserURLLoaderThrottle::DeleteUrlChecker() {
  pending_sync_checks_ = 0;
  pending_async_checks_ = 0;
  sync_sb_checker_.reset();
  async_sb_checker_.reset();
  skip_check_checker_.reset();
}

void BrowserURLLoaderThrottle::MaybeTransferAsyncChecker() {
  // If the sync check has completed but the async check has not, move the async
  // check to AsyncCheckTracker.
  DCHECK_EQ(pending_sync_checks_, 0u);
  if (async_sb_checker_) {
    base::UmaHistogramBoolean(
        "SafeBrowsing.BrowserThrottle.IsAsyncCheckerTransferred",
        pending_async_checks_ > 0);
  }
  if (pending_async_checks_ > 0) {
    async_check_tracker_->TransferUrlChecker(std::move(async_sb_checker_));
  }
}

}  // namespace safe_browsing
