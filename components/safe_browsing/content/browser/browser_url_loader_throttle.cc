// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
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

BrowserURLLoaderThrottle::SkipCheckCheckerOnSB::SkipCheckCheckerOnSB(
    UrlCheckerOnSB::GetDelegateCallback delegate_getter,
    int frame_tree_node_id)
    : delegate_getter_(std::move(delegate_getter)),
      frame_tree_node_id_(frame_tree_node_id) {}

BrowserURLLoaderThrottle::SkipCheckCheckerOnSB::~SkipCheckCheckerOnSB() =
    default;

void BrowserURLLoaderThrottle::SkipCheckCheckerOnSB::CheckOriginalUrl(
    OnCompleteCheckCallback callback,
    const GURL& url,
    bool originated_from_service_worker) {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate =
      std::move(delegate_getter_).Run();
  should_skip_checks_ =
      !url_checker_delegate ||
      url_checker_delegate->ShouldSkipRequestCheck(
          url, frame_tree_node_id_,
          /*render_process_id=*/content::ChildProcessHost::kInvalidUniqueID,
          /*render_frame_token=*/std::nullopt, originated_from_service_worker);
  if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
    std::move(callback).Run(should_skip_checks_);
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), should_skip_checks_));
  }
}

void BrowserURLLoaderThrottle::SkipCheckCheckerOnSB::CheckRedirectUrl(
    OnCompleteCheckCallback callback) {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);
  if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
    std::move(callback).Run(should_skip_checks_);
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), should_skip_checks_));
  }
}

// static
std::unique_ptr<BrowserURLLoaderThrottle> BrowserURLLoaderThrottle::Create(
    UrlCheckerOnSB::GetDelegateCallback delegate_getter,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    int frame_tree_node_id,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
    base::WeakPtr<HashRealTimeService> hash_realtime_service,
    base::WeakPtr<PingManager> ping_manager,
    hash_realtime_utils::HashRealTimeSelection hash_realtime_selection,
    base::WeakPtr<AsyncCheckTracker> async_check_tracker) {
  return base::WrapUnique<BrowserURLLoaderThrottle>(
      new BrowserURLLoaderThrottle(
          std::move(delegate_getter), web_contents_getter, frame_tree_node_id,
          url_lookup_service, hash_realtime_service, ping_manager,
          hash_realtime_selection, async_check_tracker));
}

BrowserURLLoaderThrottle::BrowserURLLoaderThrottle(
    UrlCheckerOnSB::GetDelegateCallback delegate_getter,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    int frame_tree_node_id,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
    base::WeakPtr<HashRealTimeService> hash_realtime_service,
    base::WeakPtr<PingManager> ping_manager,
    hash_realtime_utils::HashRealTimeSelection hash_realtime_selection,
    base::WeakPtr<AsyncCheckTracker> async_check_tracker)
    : async_check_tracker_(async_check_tracker) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Decide whether to do real time URL lookups or not.
  url_real_time_lookup_enabled_ =
      url_lookup_service ? url_lookup_service->CanPerformFullURLLookup()
                         : false;

  bool can_urt_check_subresource_url =
      url_lookup_service && url_lookup_service->CanCheckSubresourceURL();

// This BUILDFLAG check is not strictly necessary because the feature should
// only be enabled for Desktop. This check is included only as a precaution and
// for clarity.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  bool is_mechanism_experiment_allowed =
      hash_realtime_service &&
      hash_realtime_service->IsEnhancedProtectionEnabled() &&
      base::FeatureList::IsEnabled(kSafeBrowsingLookupMechanismExperiment);
#else
  bool is_mechanism_experiment_allowed = false;
#endif

  // Decide whether safe browsing database can be checked.
  // If url_lookup_service is null, safe browsing database should be checked by
  // default.
  bool can_check_db =
      url_lookup_service ? url_lookup_service->CanCheckSafeBrowsingDb() : true;
  bool can_check_high_confidence_allowlist =
      url_lookup_service
          ? url_lookup_service->CanCheckSafeBrowsingHighConfidenceAllowlist()
          : true;

  url_lookup_service_metric_suffix_ =
      url_real_time_lookup_enabled_ ? url_lookup_service->GetMetricSuffix()
                                    : kNoRealTimeURLLookupService;

  sync_sb_checker_ = std::make_unique<UrlCheckerOnSB>(
      delegate_getter, frame_tree_node_id, web_contents_getter,
      /*complete_callback=*/
      base::BindRepeating(&BrowserURLLoaderThrottle::OnCompleteSyncCheck,
                          weak_factory_.GetWeakPtr()),
      /*slow_check_callback=*/
      base::BindRepeating(&BrowserURLLoaderThrottle::NotifySyncSlowCheck,
                          weak_factory_.GetWeakPtr()),
      url_real_time_lookup_enabled_, can_urt_check_subresource_url,
      can_check_db, can_check_high_confidence_allowlist,
      url_lookup_service_metric_suffix_, url_lookup_service,
      hash_realtime_service, ping_manager, is_mechanism_experiment_allowed,
      hash_realtime_selection);

  skip_check_checker_ = std::make_unique<SkipCheckCheckerOnSB>(
      std::move(delegate_getter), frame_tree_node_id);
}

BrowserURLLoaderThrottle::~BrowserURLLoaderThrottle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (deferred_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                    TRACE_ID_LOCAL(this));
  }

  DeleteUrlCheckerOnSB();
}

void BrowserURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(0u, pending_sync_checks_);
  DCHECK(!blocked_);
  base::UmaHistogramEnumeration(
      "SafeBrowsing.BrowserThrottle.RequestDestination", request->destination);

  if (KnownSafeUrl(request->url)) {
    skip_checks_ = true;
    return;
  }

  if (request->destination != network::mojom::RequestDestination::kDocument &&
      base::FeatureList::IsEnabled(kSafeBrowsingSkipSubresources)) {
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

  pending_sync_checks_++;
  start_request_time_ = base::TimeTicks::Now();
  is_start_request_called_ = true;

  if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
    skip_check_checker_->CheckOriginalUrl(
        base::BindOnce(
            &BrowserURLLoaderThrottle::OnSkipCheckCompleteOnOriginalUrl,
            weak_factory_.GetWeakPtr(), request->headers, request->load_flags,
            request->destination, request->has_user_gesture, request->url,
            request->method),
        request->url, request->originated_from_service_worker);
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SkipCheckCheckerOnSB::CheckOriginalUrl,
            skip_check_checker_->AsWeakPtr(),
            base::BindOnce(
                &BrowserURLLoaderThrottle::OnSkipCheckCompleteOnOriginalUrl,
                weak_factory_.GetWeakPtr(), request->headers,
                request->load_flags, request->destination,
                request->has_user_gesture, request->url, request->method),
            request->url, request->originated_from_service_worker));
  }
}

void BrowserURLLoaderThrottle::OnSkipCheckCompleteOnOriginalUrl(
    const net::HttpRequestHeaders& headers,
    int load_flags,
    network::mojom::RequestDestination request_destination,
    bool has_user_gesture,
    const GURL& url,
    const std::string& method,
    bool should_skip) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (should_skip) {
    SkipChecks();
    return;
  }

  if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    sync_sb_checker_->Start(headers, load_flags, request_destination,
                            has_user_gesture, url, method);
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&UrlCheckerOnSB::Start, sync_sb_checker_->AsWeakPtr(),
                       headers, load_flags, request_destination,
                       has_user_gesture, url, method));
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

  // The check to |skip_check_checker| cannot be skipped because
  // WillRedirectRequest may be called while |skip_check_checker| is still in
  // progress.
  if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
    skip_check_checker_->CheckRedirectUrl(base::BindOnce(
        &BrowserURLLoaderThrottle::OnSkipCheckCompleteOnRedirectUrl,
        weak_factory_.GetWeakPtr(), redirect_info->new_url,
        redirect_info->new_method));
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SkipCheckCheckerOnSB::CheckRedirectUrl,
            skip_check_checker_->AsWeakPtr(),
            base::BindOnce(
                &BrowserURLLoaderThrottle::OnSkipCheckCompleteOnRedirectUrl,
                weak_factory_.GetWeakPtr(), redirect_info->new_url,
                redirect_info->new_method)));
  }
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

  if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    sync_sb_checker_->CheckUrl(url, method);
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&UrlCheckerOnSB::CheckUrl,
                                  sync_sb_checker_->AsWeakPtr(), url, method));
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

  if (sync_sb_checker_) {
    if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
      sync_sb_checker_->LogWillProcessResponseTime(base::TimeTicks::Now());
    } else {
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&UrlCheckerOnSB::LogWillProcessResponseTime,
                                    sync_sb_checker_->AsWeakPtr(),
                                    base::TimeTicks::Now()));
    }
  }

  if (blocked_) {
    // OnCheckUrlResult() has set |blocked_| to true and called
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

UrlCheckerOnSB* BrowserURLLoaderThrottle::GetSyncSBCheckerForTesting() {
  return sync_sb_checker_.get();
}

void BrowserURLLoaderThrottle::OnCompleteSyncCheck(
    bool slow_check,
    bool proceed,
    bool showed_interstitial,
    SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!blocked_);
  DCHECK(url_real_time_lookup_enabled_ ||
         performed_check !=
             SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck);

  DCHECK_LT(0u, pending_sync_checks_);
  pending_sync_checks_--;

  if (slow_check) {
    DCHECK_LT(0u, pending_sync_slow_checks_);
    pending_sync_slow_checks_--;
  }

  // If the resource load is going to finish (either being cancelled or
  // resumed), record the total delay.
  if (!proceed || pending_sync_checks_ == 0) {
    // If the resource load is currently deferred, there is a delay.
    if (deferred_) {
      total_delay_ = base::TimeTicks::Now() - defer_start_time_;
      LogTotalDelay2MetricsWithResponseType(is_response_from_cache_,
                                            total_delay_);
    }
    LogTotalDelay2Metrics(GetUrlCheckTypeForLogging(performed_check),
                          total_delay_);
  }

  if (proceed) {
    if (pending_sync_slow_checks_ == 0 && slow_check) {
      delegate_->ResumeReadingBodyFromNet();
    }

    if (pending_sync_checks_ == 0 && deferred_) {
      deferred_ = false;
      TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                      TRACE_ID_LOCAL(this));
      base::UmaHistogramTimes("SafeBrowsing.BrowserThrottle.TotalDelay",
                              total_delay_);
      delegate_->Resume();
    }
  } else {
    blocked_ = true;

    DeleteUrlCheckerOnSB();
    pending_sync_checks_ = 0;
    pending_sync_slow_checks_ = 0;
    // If we didn't show an interstitial, we cancel with ERR_ABORTED to not show
    // an error page either.
    delegate_->CancelWithError(
        showed_interstitial ? kNetErrorCodeForSafeBrowsing : net::ERR_ABORTED,
        kCustomCancelReasonForURLLoader);
  }
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
      NOTREACHED();
      return ".HashPrefixDatabaseCheck";
  }
}

void BrowserURLLoaderThrottle::SkipChecks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Future checks for redirects will be skipped.
  skip_checks_ = true;

  pending_sync_checks_--;
  if (pending_sync_checks_ == 0 && deferred_) {
    delegate_->Resume();
  }
}

void BrowserURLLoaderThrottle::NotifySyncSlowCheck() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pending_sync_slow_checks_++;

  // Pending slow checks indicate that the resource may be unsafe. In that case,
  // pause reading response body from network to minimize the chance of
  // processing unsafe contents (e.g., writing unsafe contents into cache),
  // until we get the results. According to the results, we may resume reading
  // or cancel the resource load.
  // For real time Safe Browsing checks, we continue reading the response body
  // but, similar to hash-based checks, do not process it until we know it is
  // SAFE.
  if (pending_sync_slow_checks_ == 1) {
    delegate_->PauseReadingBodyFromNet();
  }
}

void BrowserURLLoaderThrottle::DeleteUrlCheckerOnSB() {
  if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    sync_sb_checker_.reset();
    skip_check_checker_.reset();
  } else {
    content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                   std::move(sync_sb_checker_));
    content::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, std::move(skip_check_checker_));
  }
}

}  // namespace safe_browsing
