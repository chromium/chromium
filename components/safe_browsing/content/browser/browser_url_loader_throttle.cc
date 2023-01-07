// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/web_contents.h"
#include "net/log/net_log_event_type.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

// TODO(http://crbug.com/824843): Remove this if safe browsing is moved to the
// UI thread.
class BrowserURLLoaderThrottle::CheckerOnIO
    : public base::SupportsWeakPtr<BrowserURLLoaderThrottle::CheckerOnIO> {
 public:
  CheckerOnIO(
      GetDelegateCallback delegate_getter,
      int frame_tree_node_id,
      base::RepeatingCallback<content::WebContents*()> web_contents_getter,
      base::WeakPtr<BrowserURLLoaderThrottle> throttle,
      bool real_time_lookup_enabled,
      bool can_rt_check_subresource_url,
      bool can_check_db,
      bool can_check_high_confidence_allowlist,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service)
      : delegate_getter_(std::move(delegate_getter)),
        frame_tree_node_id_(frame_tree_node_id),
        web_contents_getter_(web_contents_getter),
        throttle_(std::move(throttle)),
        real_time_lookup_enabled_(real_time_lookup_enabled),
        can_rt_check_subresource_url_(can_rt_check_subresource_url),
        can_check_db_(can_check_db),
        can_check_high_confidence_allowlist_(
            can_check_high_confidence_allowlist),
        url_lookup_service_(url_lookup_service) {
    content::WebContents* contents = web_contents_getter_.Run();
    if (!!contents) {
      last_committed_url_ = contents->GetLastCommittedURL();
    }
  }

  // Starts the initial safe browsing check. This check and future checks may be
  // skipped after checking with the UrlCheckerDelegate.
  void Start(const net::HttpRequestHeaders& headers,
             int load_flags,
             network::mojom::RequestDestination request_destination,
             bool has_user_gesture,
             bool originated_from_service_worker,
             const GURL& url,
             const std::string& method) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    scoped_refptr<UrlCheckerDelegate> url_checker_delegate =
        std::move(delegate_getter_).Run();
    skip_checks_ =
        !url_checker_delegate ||
        url_checker_delegate->ShouldSkipRequestCheck(
            url, frame_tree_node_id_,
            content::ChildProcessHost::kInvalidUniqueID /* render_process_id */,
            MSG_ROUTING_NONE /* render_frame_id */,
            originated_from_service_worker);
    if (skip_checks_) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&BrowserURLLoaderThrottle::SkipChecks, throttle_));
      return;
    }

    url_checker_ = std::make_unique<SafeBrowsingUrlCheckerImpl>(
        headers, load_flags, request_destination, has_user_gesture,
        url_checker_delegate, web_contents_getter_,
        content::ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE,
        frame_tree_node_id_, real_time_lookup_enabled_,
        can_rt_check_subresource_url_, can_check_db_,
        can_check_high_confidence_allowlist_, last_committed_url_,
        content::GetUIThreadTaskRunner({}), url_lookup_service_,
        WebUIInfoSingleton::GetInstance());

    CheckUrl(url, method);
  }

  // Checks the specified |url| using |url_checker_|.
  void CheckUrl(const GURL& url, const std::string& method) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (skip_checks_) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&BrowserURLLoaderThrottle::SkipChecks, throttle_));
      return;
    }

    DCHECK(url_checker_);
    url_checker_->CheckUrl(
        url, method,
        base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnIO::OnCheckUrlResult,
                       base::Unretained(this)));
  }

 private:
  // If |slow_check_notifier| is non-null, it indicates that a "slow check" is
  // ongoing, i.e., the URL may be unsafe and a more time-consuming process is
  // required to get the final result. In that case, the rest of the callback
  // arguments should be ignored. This method sets the |slow_check_notifier|
  // output parameter to a callback to receive the final result.
  void OnCheckUrlResult(NativeUrlCheckNotifier* slow_check_notifier,
                        bool proceed,
                        bool showed_interstitial) {
    if (!slow_check_notifier) {
      OnCompleteCheck(false /* slow_check */, proceed, showed_interstitial);
      return;
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BrowserURLLoaderThrottle::NotifySlowCheck, throttle_));

    // In this case |proceed| and |showed_interstitial| should be ignored. The
    // result will be returned by calling |*slow_check_notifier| callback.
    *slow_check_notifier =
        base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnIO::OnCompleteCheck,
                       base::Unretained(this), true /* slow_check */);
  }

  // |slow_check| indicates whether it reports the result of a slow check.
  // (Please see comments of OnCheckUrlResult() for what slow check means).
  void OnCompleteCheck(bool slow_check,
                       bool proceed,
                       bool showed_interstitial) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BrowserURLLoaderThrottle::OnCompleteCheck, throttle_,
                       slow_check, proceed, showed_interstitial));
  }

  // The following member stays valid until |url_checker_| is created.
  GetDelegateCallback delegate_getter_;

  std::unique_ptr<SafeBrowsingUrlCheckerImpl> url_checker_;
  int frame_tree_node_id_;
  base::RepeatingCallback<content::WebContents*()> web_contents_getter_;
  bool skip_checks_ = false;
  base::WeakPtr<BrowserURLLoaderThrottle> throttle_;
  bool real_time_lookup_enabled_ = false;
  bool can_rt_check_subresource_url_ = false;
  bool can_check_db_ = true;
  bool can_check_high_confidence_allowlist_ = true;
  GURL last_committed_url_;
  base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_;
};

// static
std::unique_ptr<BrowserURLLoaderThrottle> BrowserURLLoaderThrottle::Create(
    GetDelegateCallback delegate_getter,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    int frame_tree_node_id,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service) {
  return base::WrapUnique<BrowserURLLoaderThrottle>(
      new BrowserURLLoaderThrottle(std::move(delegate_getter),
                                   web_contents_getter, frame_tree_node_id,
                                   url_lookup_service));
}

BrowserURLLoaderThrottle::BrowserURLLoaderThrottle(
    GetDelegateCallback delegate_getter,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    int frame_tree_node_id,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Decide whether to do real time URL lookups or not.
  bool real_time_lookup_enabled =
      url_lookup_service ? url_lookup_service->CanPerformFullURLLookup()
                         : false;

  bool can_rt_check_subresource_url =
      url_lookup_service && url_lookup_service->CanCheckSubresourceURL();

  // Decide whether safe browsing database can be checked.
  // If url_lookup_service is null, safe browsing database should be checked by
  // default.
  bool can_check_db =
      url_lookup_service ? url_lookup_service->CanCheckSafeBrowsingDb() : true;
  bool can_check_high_confidence_allowlist =
      url_lookup_service
          ? url_lookup_service->CanCheckSafeBrowsingHighConfidenceAllowlist()
          : true;
  io_checker_ = std::make_unique<CheckerOnIO>(
      std::move(delegate_getter), frame_tree_node_id, web_contents_getter,
      weak_factory_.GetWeakPtr(), real_time_lookup_enabled,
      can_rt_check_subresource_url, can_check_db,
      can_check_high_confidence_allowlist, url_lookup_service);
}

BrowserURLLoaderThrottle::~BrowserURLLoaderThrottle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (deferred_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                    TRACE_ID_LOCAL(this));
  }

  DeleteCheckerOnIO();
}

void BrowserURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(0u, pending_checks_);
  DCHECK(!blocked_);

  original_url_ = request->url;
  pending_checks_++;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnIO::Start,
                                io_checker_->AsWeakPtr(), request->headers,
                                request->load_flags, request->destination,
                                request->has_user_gesture,
                                request->originated_from_service_worker,
                                request->url, request->method));
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

  if (skip_checks_)
    return;

  pending_checks_++;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnIO::CheckUrl,
                     io_checker_->AsWeakPtr(), redirect_info->new_url,
                     redirect_info->new_method));
}

void BrowserURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (blocked_) {
    // OnCheckUrlResult() has set |blocked_| to true and called
    // |delegate_->CancelWithError|, but this method is called before the
    // request is actually cancelled. In that case, simply defer the request.
    *defer = true;
    return;
  }

  bool check_completed = (pending_checks_ == 0);
  base::UmaHistogramBoolean(
      "SafeBrowsing.BrowserThrottle.IsCheckCompletedOnProcessResponse",
      check_completed);

  if (check_completed)
    return;

  DCHECK(!deferred_);
  deferred_ = true;
  defer_start_time_ = base::TimeTicks::Now();
  *defer = true;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("safe_browsing", "Deferred",
                                    TRACE_ID_LOCAL(this), "original_url",
                                    original_url_.spec());
}

const char* BrowserURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "SafeBrowsingBrowserThrottle";
}

void BrowserURLLoaderThrottle::OnCompleteCheck(bool slow_check,
                                               bool proceed,
                                               bool showed_interstitial) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!blocked_);

  DCHECK_LT(0u, pending_checks_);
  pending_checks_--;

  if (slow_check) {
    DCHECK_LT(0u, pending_slow_checks_);
    pending_slow_checks_--;
  }

  // If the resource load is currently deferred and is going to exit that state
  // (either being cancelled or resumed), record the total delay.
  if (deferred_ && (!proceed || pending_checks_ == 0))
    total_delay_ = base::TimeTicks::Now() - defer_start_time_;

  if (proceed) {
    if (pending_slow_checks_ == 0 && slow_check)
      delegate_->ResumeReadingBodyFromNet();

    if (pending_checks_ == 0 && deferred_) {
      deferred_ = false;
      TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                      TRACE_ID_LOCAL(this));
      base::UmaHistogramTimes("SafeBrowsing.BrowserThrottle.TotalDelay",
                              total_delay_);
      delegate_->Resume();
    }
  } else {
    blocked_ = true;

    DeleteCheckerOnIO();
    pending_checks_ = 0;
    pending_slow_checks_ = 0;
    // If we didn't show an interstitial, we cancel with ERR_ABORTED to not show
    // an error page either.
    delegate_->CancelWithError(
        showed_interstitial ? kNetErrorCodeForSafeBrowsing : net::ERR_ABORTED,
        kCustomCancelReasonForURLLoader);
  }
}

void BrowserURLLoaderThrottle::SkipChecks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Future checks for redirects will be skipped.
  skip_checks_ = true;

  pending_checks_--;
  if (pending_checks_ == 0 && deferred_)
    delegate_->Resume();
}

void BrowserURLLoaderThrottle::NotifySlowCheck() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pending_slow_checks_++;

  // Pending slow checks indicate that the resource may be unsafe. In that case,
  // pause reading response body from network to minimize the chance of
  // processing unsafe contents (e.g., writing unsafe contents into cache),
  // until we get the results. According to the results, we may resume reading
  // or cancel the resource load.
  // For real time Safe Browsing checks, we continue reading the response body
  // but, similar to hash-based checks, do not process it until we know it is
  // SAFE.
  if (pending_slow_checks_ == 1)
    delegate_->PauseReadingBodyFromNet();
}

void BrowserURLLoaderThrottle::DeleteCheckerOnIO() {
  content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                 std::move(io_checker_));
}

}  // namespace safe_browsing
