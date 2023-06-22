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

class UrlCheckerDelegate;
class SafeBrowsingLookupMechanismExperimenter;

class RealTimeUrlLookupServiceBase;
class HashRealTimeService;
class PingManager;

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
  using GetDelegateCallback =
      base::OnceCallback<scoped_refptr<UrlCheckerDelegate>()>;

  using NativeUrlCheckNotifier = base::OnceCallback<void(
      bool /* proceed */,
      bool /* showed_interstitial */,
      SafeBrowsingUrlCheckerImpl::PerformedCheck /* performed_check */,
      bool /* did_check_url_real_time_allowlist */)>;

  // CheckerOnSB handles calling methods on SafeBrowsingUrlCheckerImpl, which
  // must be called on the IO thread. The results are synced back to the
  // throttle.
  // TODO(http://crbug.com/824843): Remove this if safe browsing is moved to the
  // UI thread.
  class CheckerOnSB
      : public base::SupportsWeakPtr<BrowserURLLoaderThrottle::CheckerOnSB> {
   public:
    CheckerOnSB(
        GetDelegateCallback delegate_getter,
        int frame_tree_node_id,
        base::RepeatingCallback<content::WebContents*()> web_contents_getter,
        base::WeakPtr<BrowserURLLoaderThrottle> throttle,
        bool url_real_time_lookup_enabled,
        bool can_urt_check_subresource_url,
        bool can_check_db,
        bool can_check_high_confidence_allowlist,
        std::string url_lookup_service_metric_suffix,
        base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
        base::WeakPtr<HashRealTimeService> hash_realtime_service,
        base::WeakPtr<PingManager> ping_manager,
        bool is_mechanism_experiment_allowed,
        hash_realtime_utils::HashRealTimeSelection hash_realtime_selection);

    ~CheckerOnSB();

    // Starts the initial safe browsing check. This check and future checks may
    // be skipped after checking with the UrlCheckerDelegate.
    void Start(const net::HttpRequestHeaders& headers,
               int load_flags,
               network::mojom::RequestDestination request_destination,
               bool has_user_gesture,
               bool originated_from_service_worker,
               const GURL& url,
               const std::string& method);

    // Checks the specified |url| using |url_checker_|.
    void CheckUrl(const GURL& url, const std::string& method);

    void LogWillProcessResponseTime(base::TimeTicks reached_time);

    void SetUrlCheckerForTesting(
        std::unique_ptr<SafeBrowsingUrlCheckerImpl> checker);

   private:
    // If |slow_check_notifier| is non-null, it indicates that a "slow check" is
    // ongoing, i.e., the URL may be unsafe and a more time-consuming process is
    // required to get the final result. In that case, the rest of the callback
    // arguments should be ignored. This method sets the |slow_check_notifier|
    // output parameter to a callback to receive the final result.
    void OnCheckUrlResult(
        NativeUrlCheckNotifier* slow_check_notifier,
        bool proceed,
        bool showed_interstitial,
        SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check,
        bool did_check_url_real_time_allowlist);

    // |slow_check| indicates whether it reports the result of a slow check.
    // (Please see comments of OnCheckUrlResult() for what slow check means).
    void OnCompleteCheck(
        bool slow_check,
        bool proceed,
        bool showed_interstitial,
        SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check,
        bool did_check_url_real_time_allowlist);

    // The following member stays valid until |url_checker_| is created.
    GetDelegateCallback delegate_getter_;

    std::unique_ptr<SafeBrowsingUrlCheckerImpl> url_checker_;
    std::unique_ptr<SafeBrowsingUrlCheckerImpl> url_checker_for_testing_;
    int frame_tree_node_id_;
    scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
        mechanism_experimenter_;
    base::RepeatingCallback<content::WebContents*()> web_contents_getter_;
    bool skip_checks_ = false;
    base::WeakPtr<BrowserURLLoaderThrottle> throttle_;
    bool url_real_time_lookup_enabled_ = false;
    bool can_urt_check_subresource_url_ = false;
    bool can_check_db_ = true;
    bool can_check_high_confidence_allowlist_ = true;
    std::string url_lookup_service_metric_suffix_;
    GURL last_committed_url_;
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_;
    base::WeakPtr<HashRealTimeService> hash_realtime_service_;
    base::WeakPtr<PingManager> ping_manager_;
    bool is_mechanism_experiment_allowed_ = false;
    hash_realtime_utils::HashRealTimeSelection hash_realtime_selection_ =
        hash_realtime_utils::HashRealTimeSelection::kNone;
    base::TimeTicks creation_time_;
  };

  static std::unique_ptr<BrowserURLLoaderThrottle> Create(
      GetDelegateCallback delegate_getter,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      int frame_tree_node_id,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
      base::WeakPtr<HashRealTimeService> hash_realtime_service,
      base::WeakPtr<PingManager> ping_manager,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection);

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

  CheckerOnSB* GetSBCheckerForTesting();

 private:
  // |web_contents_getter| is used for displaying SafeBrowsing UI when
  // necessary.
  BrowserURLLoaderThrottle(
      GetDelegateCallback delegate_getter,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      int frame_tree_node_id,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
      base::WeakPtr<HashRealTimeService> hash_realtime_service,
      base::WeakPtr<PingManager> ping_manager,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection);

  // |slow_check| indicates whether it reports the result of a slow check.
  // (Please see comments of CheckerOnSB::OnCheckUrlResult() for what slow check
  // means).
  void OnCompleteCheck(
      bool slow_check,
      bool proceed,
      bool showed_interstitial,
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check,
      bool did_check_url_real_time_allowlist);

  // Returns the suffixed to be used for the TotalDelay2 metrics that specifies
  // which type of check was performed.
  std::string GetUrlCheckTypeForLogging(
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check);

  // Called to skip future safe browsing checks and resume the request if
  // necessary.
  void SkipChecks();

  // Called when a slow safe browsing check is ongoing.
  void NotifySlowCheck();

  // Destroys |sb_checker_| on the IO thread, or UI thread if
  // kSafeBrowsingOnUIThread is enabled.
  void DeleteCheckerOnSB();

  size_t pending_checks_ = 0;
  // How many slow checks that haven't received results.
  size_t pending_slow_checks_ = 0;
  bool blocked_ = false;

  // The time when |WillStartRequest| is called.
  base::TimeTicks start_request_time_;
  bool is_start_request_called_ = false;

  // The time when we started deferring the request.
  base::TimeTicks defer_start_time_;
  bool deferred_ = false;
  // Whether the response loaded is from cache.
  bool is_response_from_cache_ = false;
  network::mojom::RequestDestination request_destination_;

  // The total delay caused by SafeBrowsing deferring the resource load.
  base::TimeDelta total_delay_;

  GURL original_url_;

  // Whether future safe browsing checks should be skipped.
  bool skip_checks_ = false;

  std::unique_ptr<CheckerOnSB> sb_checker_;

  // Metric suffix for the URL lookup service.
  std::string url_lookup_service_metric_suffix_;

  // Whether real time URL lookup is enabled for the user.
  bool url_real_time_lookup_enabled_;

  // Tracks how many times |WillProcessResponse| is called.
  int will_process_response_count_ = 0;

  base::WeakPtrFactory<BrowserURLLoaderThrottle> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_BROWSER_URL_LOADER_THROTTLE_H_
