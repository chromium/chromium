// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_BROWSER_URL_LOADER_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_BROWSER_URL_LOADER_THROTTLE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/safe_browsing/content/browser/url_checker_on_sb.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace net {
class HttpRequestHeaders;
}

namespace safe_browsing {

class RealTimeUrlLookupServiceBase;
class HashRealTimeService;
class PingManager;
class AsyncCheckTracker;

// BrowserURLLoaderThrottle is used in the browser process to query
// SafeBrowsing to determine whether a URL and also its redirect URLs are safe
// to load.
//
// This throttle never defers starting the URL request or following redirects,
// no matter on mobile or desktop. If any of the checks for the original URL
// and redirect chain are not complete by the time the response headers are
// available, the request is deferred until all the checks are done. It cancels
// the load if any URLs turn out to be bad.
class BrowserURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  // Helper class to perform whether the check can be skipped on the SB thread.
  class SkipCheckCheckerOnSB
      : public base::SupportsWeakPtr<
            BrowserURLLoaderThrottle::SkipCheckCheckerOnSB> {
   public:
    using OnCompleteCheckCallback =
        base::OnceCallback<void(bool /* should_skip */)>;

    SkipCheckCheckerOnSB(UrlCheckerOnSB::GetDelegateCallback delegate_getter,
                         int frame_tree_node_id);
    ~SkipCheckCheckerOnSB();

    void CheckOriginalUrl(OnCompleteCheckCallback callback,
                          const GURL& url,
                          bool originated_from_service_worker);
    void CheckRedirectUrl(OnCompleteCheckCallback callback);

   private:
    UrlCheckerOnSB::GetDelegateCallback delegate_getter_;
    int frame_tree_node_id_;
    bool should_skip_checks_ = false;
  };

  static std::unique_ptr<BrowserURLLoaderThrottle> Create(
      UrlCheckerOnSB::GetDelegateCallback delegate_getter,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      int frame_tree_node_id,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
      base::WeakPtr<HashRealTimeService> hash_realtime_service,
      base::WeakPtr<PingManager> ping_manager,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection,
      base::WeakPtr<AsyncCheckTracker> async_check_tracker);

  BrowserURLLoaderThrottle(const BrowserURLLoaderThrottle&) = delete;
  BrowserURLLoaderThrottle& operator=(const BrowserURLLoaderThrottle&) = delete;

  ~BrowserURLLoaderThrottle() override;

  // blink::URLLoaderThrottle implementation.
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  const char* NameForLoggingWillProcessResponse() override;

  UrlCheckerOnSB* GetSyncSBCheckerForTesting();

 private:
  // |web_contents_getter| is used for displaying SafeBrowsing UI when
  // necessary.
  BrowserURLLoaderThrottle(
      UrlCheckerOnSB::GetDelegateCallback delegate_getter,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      int frame_tree_node_id,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
      base::WeakPtr<HashRealTimeService> hash_realtime_service,
      base::WeakPtr<PingManager> ping_manager,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection,
      base::WeakPtr<AsyncCheckTracker> async_check_tracker);

  void OnSkipCheckCompleteOnOriginalUrl(
      const net::HttpRequestHeaders& headers,
      int load_flags,
      network::mojom::RequestDestination request_destination,
      bool has_user_gesture,
      const GURL& url,
      const std::string& method,
      bool should_skip);
  void OnSkipCheckCompleteOnRedirectUrl(const GURL& url,
                                        const std::string& method,
                                        bool should_skip);

  // |slow_check| indicates whether it reports the result of a slow check.
  // (Please see comments of UrlCheckerOnSB::OnCheckUrlResult() for what slow
  // check means).
  void OnCompleteSyncCheck(
      bool slow_check,
      bool proceed,
      bool showed_interstitial,
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check);

  // Called to skip future safe browsing checks and resume the request if
  // necessary.
  void SkipChecks();

  // Called when a slow safe browsing check is ongoing.
  void NotifySyncSlowCheck();

  // Returns the suffixed to be used for the TotalDelay2 metrics that specifies
  // which type of check was performed.
  std::string GetUrlCheckTypeForLogging(
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check);

  // Destroys all checkers on the IO thread, or UI thread if
  // kSafeBrowsingOnUIThread is enabled.
  void DeleteUrlCheckerOnSB();

  size_t pending_sync_checks_ = 0;

  // How many slow checks that haven't received results.
  size_t pending_sync_slow_checks_ = 0;

  // Whether future safe browsing checks should be skipped.
  bool skip_checks_ = false;

  bool blocked_ = false;

  // The time when |WillStartRequest| is called.
  base::TimeTicks start_request_time_;
  bool is_start_request_called_ = false;

  // The time when we started deferring the request.
  base::TimeTicks defer_start_time_;
  bool deferred_ = false;
  // Whether the response loaded is from cache.
  bool is_response_from_cache_ = false;

  // The total delay caused by SafeBrowsing deferring the resource load.
  base::TimeDelta total_delay_;

  std::unique_ptr<UrlCheckerOnSB> sync_sb_checker_;

  // Used to decide whether the check can be skipped on the SB thread.
  std::unique_ptr<SkipCheckCheckerOnSB> skip_check_checker_;

  // Metric suffix for the URL lookup service.
  std::string url_lookup_service_metric_suffix_;

  // Whether real time URL lookup is enabled for the user.
  bool url_real_time_lookup_enabled_;

  // Tracks how many times |WillProcessResponse| is called.
  int will_process_response_count_ = 0;

  // In progress async SB checker will be transferred to this object.
  base::WeakPtr<AsyncCheckTracker> async_check_tracker_;

  base::WeakPtrFactory<BrowserURLLoaderThrottle> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_BROWSER_URL_LOADER_THROTTLE_H_
