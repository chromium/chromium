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
#include "components/safe_browsing/content/browser/url_checker_holder.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "content/public/browser/browser_thread.h"
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
  // Helper class to perform whether the check can be skipped.
  class SkipCheckChecker final {
   public:
    using OnCompleteCheckCallback =
        base::OnceCallback<void(bool /* should_skip */)>;

    SkipCheckChecker(UrlCheckerHolder::GetDelegateCallback delegate_getter,
                     content::FrameTreeNodeId frame_tree_node_id);
    ~SkipCheckChecker();

    void CheckOriginalUrl(OnCompleteCheckCallback callback,
                          const GURL& url,
                          bool originated_from_service_worker);
    void CheckRedirectUrl(OnCompleteCheckCallback callback);

    base::WeakPtr<SkipCheckChecker> AsWeakPtr() {
      return weak_factory_.GetWeakPtr();
    }

   private:
    UrlCheckerHolder::GetDelegateCallback delegate_getter_;
    content::FrameTreeNodeId frame_tree_node_id_;
    bool should_skip_checks_ = false;
    base::WeakPtrFactory<SkipCheckChecker> weak_factory_{this};
  };

  static std::unique_ptr<BrowserURLLoaderThrottle> Create(
      UrlCheckerHolder::GetDelegateCallback delegate_getter,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      content::FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
      base::WeakPtr<HashRealTimeService> hash_realtime_service,
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

  UrlCheckerHolder* GetSyncSBCheckerForTesting();
  UrlCheckerHolder* GetAsyncSBCheckerForTesting();
  void SetOnSyncSBCheckerCreatedCallbackForTesting(base::OnceClosure callback);
  void SetOnAsyncSBCheckerCreatedCallbackForTesting(base::OnceClosure callback);

 private:
  // |web_contents_getter| is used for displaying SafeBrowsing UI when
  // necessary.
  BrowserURLLoaderThrottle(
      UrlCheckerHolder::GetDelegateCallback delegate_getter,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      content::FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
      base::WeakPtr<HashRealTimeService> hash_realtime_service,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection,
      base::WeakPtr<AsyncCheckTracker> async_check_tracker);

  void OnSkipCheckCompleteOnOriginalUrl(
      const net::HttpRequestHeaders& headers,
      int load_flags,
      bool has_user_gesture,
      const GURL& url,
      const std::string& method,
      bool should_skip);
  void OnSkipCheckCompleteOnRedirectUrl(const GURL& url,
                                        const std::string& method,
                                        bool should_skip);

  void OnCompleteSyncCheck(UrlCheckerHolder::OnCompleteCheckResult result);
  void OnCompleteAsyncCheck(UrlCheckerHolder::OnCompleteCheckResult result);

  // Called to skip future safe browsing checks and resume the request if
  // necessary.
  void SkipChecks();

  // Returns the suffixed to be used for the TotalDelay2 metrics that specifies
  // which type of check was performed.
  std::string GetUrlCheckTypeForLogging(
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check);

  // Called when the URL is identified as dangerous.
  void BlockUrlLoader(bool showed_interstitial);

  // Destroys all checkers.
  void DeleteUrlChecker();

  // If |sync_sb_checker_| has completed, but |async_sb_checker_| has not,
  // transfer the ownership of |async_sb_checker_| to |async_check_tracker_|.
  void MaybeTransferAsyncChecker();

  size_t pending_sync_checks_ = 0;
  size_t pending_async_checks_ = 0;

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

  // When async checks are eligible, this is set either to true or false when
  // one of |pending_sync_checks_| or |pending_async_checks_| is decremented to
  // 0, unless they are both set to 0 at once, in which case it is considered a
  // tie and the value remains |std::nullopt|. It is reset to |std::nullopt| any
  // time the pending check counters are incremented. This value is used for
  // logging purposes only.
  std::optional<bool> was_async_faster_than_sync_;

  // Used to decide whether the check can be skipped.
  std::unique_ptr<SkipCheckChecker> skip_check_checker_;

  // Metric suffix for the URL lookup service.
  std::string url_lookup_service_metric_suffix_;

  // Whether real time URL lookup is enabled for the user.
  bool url_real_time_lookup_enabled_;

  // Tracks how many times |WillProcessResponse| is called.
  int will_process_response_count_ = 0;

  // In progress async SB checker will be transferred to this object.
  base::WeakPtr<AsyncCheckTracker> async_check_tracker_;

  // This object is used to perform real time url check. Can only be accessed in
  // UI thread.
  base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_;

  // This object is used to perform the hash-prefix real-time lookup. It can
  // only be accessed on the UI thread.
  base::WeakPtr<HashRealTimeService> hash_realtime_service_;

  // What kind of hash-prefix real-time lookup is enabled for this request.
  hash_realtime_utils::HashRealTimeSelection hash_realtime_selection_;

  content::FrameTreeNodeId frame_tree_node_id_;
  std::optional<int64_t> navigation_id_;
  UrlCheckerHolder::GetDelegateCallback delegate_getter_;
  base::RepeatingCallback<content::WebContents*()> web_contents_getter_;
  SessionID tab_id_ = SessionID::InvalidValue();

  // Checkers used to perform Safe Browsing checks. |sync_sb_checker_| may defer
  // the URL loader. |async_sb_checker_| doesn't defer the URL loader and may
  // be transferred to |skip_check_checker_| if it is not completed.
  // |async_sb_checker_| may be null when this loader is not eligible for async
  // check. These checkers are null until |WillStartRequest| is called.
  std::unique_ptr<UrlCheckerHolder> sync_sb_checker_;
  std::unique_ptr<UrlCheckerHolder> async_sb_checker_;

  base::OnceClosure on_sync_sb_checker_created_callback_for_testing_;
  base::OnceClosure on_async_sb_checker_created_callback_for_testing_;

  base::WeakPtrFactory<BrowserURLLoaderThrottle> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_BROWSER_URL_LOADER_THROTTLE_H_
