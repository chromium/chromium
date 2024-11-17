// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_runner.h"
#include "components/safe_browsing/core/browser/url_realtime_mechanism.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_url_checker.mojom.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

class UrlCheckerDelegate;

class RealTimeUrlLookupServiceBase;

// A SafeBrowsingUrlCheckerImpl instance is used to perform SafeBrowsing check
// for a URL and its redirect URLs. It implements Mojo interface so that it can
// be used to handle queries from renderers. But it is also used to handle
// queries from the browser. In that case, the public methods are called
// directly instead of through Mojo.
//
// To be considered "safe", a URL must not appear in the SafeBrowsing blocklists
// (see SafeBrowsingService for details).
//
// Note that the SafeBrowsing check takes at most kCheckUrlTimeoutMs (defined in
// SafeBrowsingLookupMechanismRunner) milliseconds. If it takes longer than
// this, then the system defaults to treating the URL as safe.
//
// If the URL is classified as dangerous, a warning interstitial page is
// displayed. In that case, the user can click through the warning page if they
// decides to procced with loading the URL anyway.
class SafeBrowsingUrlCheckerImpl : public mojom::SafeBrowsingUrlChecker {
 public:
  // Specifies which type of check was performed.
  enum class PerformedCheck {
    kUnknown = 0,
    // A skipped check occurs in the following scenarios:
    //  - The URL is allowlisted (not the high-confidence allowlist).
    //  - It's a debugging URL like chrome://safe-browsing/match?type=malware.
    //  - The URL real-time check is unable to run, but |can_check_db_| is false
    //  so no other checks can run either.
    kCheckSkipped = 1,
    // The check that ran was a hash database check.
    kHashDatabaseCheck = 2,
    // The check that was initiated was a URL real-time check, whether or not
    // it ended up falling back to a hash database check.
    kUrlRealTimeCheck = 3,
    // The check that was initiated was a hash real-time check, whether or not
    // it ended up falling back to a hash database check.
    kHashRealTimeCheck = 4,
  };

  using NativeCheckUrlCallback =
      base::OnceCallback<void(bool /* proceed */,
                              bool /* showed_interstitial */,
                              bool /* has_post_commit_interstitial_skipped */,
                              PerformedCheck /* performed_check */)>;

  // Constructor for SafeBrowsingUrlCheckerImpl. |url_real_time_lookup_enabled|
  // indicates whether or not the profile has enabled real time URL lookups, as
  // computed by the RealTimePolicyEngine. |hash_realtime_selection| indicates
  // which type of hash-prefix real-time lookup the profile is eligible for, if
  // any. These two must be computed in advance, since this class only exists
  // on the IO thread.
  // |webui_delegate_| is allowed to be null. If non-null, it must outlive this
  // object.
  // TODO(crbug.com/40704516): Add an iOS-specific WebUIDelegate implementation
  // and pass it here to log URT requests/responses on open
  // chrome://safe-browsing pages once chrome://safe-browsing works on iOS, or
  // else to log those requests/responses to stderr.
  SafeBrowsingUrlCheckerImpl(
      const net::HttpRequestHeaders& headers,
      int load_flags,
      bool has_user_gesture,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      base::WeakPtr<web::WebState> weak_web_state,
      security_interstitials::UnsafeResource::RenderProcessId render_process_id,
      const security_interstitials::UnsafeResource::RenderFrameToken&
          render_frame_token,
      security_interstitials::UnsafeResource::FrameTreeNodeId
          frame_tree_node_id,
      std::optional<int64_t> navigation_id,
      bool url_real_time_lookup_enabled,
      bool can_check_db,
      bool can_check_high_confidence_allowlist,
      std::string url_lookup_service_metric_suffix,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      base::WeakPtr<HashRealTimeService> hash_realtime_service_on_ui,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection,
      bool is_async_check,
      bool check_allowlist_before_hash_database,
      SessionID tab_id);

  SafeBrowsingUrlCheckerImpl(const SafeBrowsingUrlCheckerImpl&) = delete;
  SafeBrowsingUrlCheckerImpl& operator=(const SafeBrowsingUrlCheckerImpl&) =
      delete;

  ~SafeBrowsingUrlCheckerImpl() override;

  // mojom::SafeBrowsingUrlChecker implementation.
  // NOTE: |callback| could be run synchronously before this method returns. Be
  // careful if |callback| could destroy this object.
  void CheckUrl(const GURL& url,
                const std::string& method,
                CheckUrlCallback callback) override;

  // NOTE: |callback| could be run synchronously before this method returns. Be
  // careful if |callback| could destroy this object.
  virtual void CheckUrl(const GURL& url,
                        const std::string& method,
                        NativeCheckUrlCallback callback);

  base::WeakPtr<SafeBrowsingUrlCheckerImpl> WeakPtr();

 protected:
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate() {
    return url_checker_delegate_.get();
  }

  base::WeakPtr<web::WebState> web_state() { return weak_web_state_; }

 private:
  using CompleteCheckResult = SafeBrowsingLookupMechanism::CompleteCheckResult;

  class Notifier {
   public:
    explicit Notifier(CheckUrlCallback callback);
    explicit Notifier(NativeCheckUrlCallback native_callback);

    ~Notifier();

    Notifier(Notifier&& other);
    Notifier& operator=(Notifier&& other);

    void OnCompleteCheck(bool proceed,
                         bool showed_interstitial,
                         bool has_post_commit_interstitial_skipped,
                         PerformedCheck performed_check);

   private:
    // Used in the mojo interface case.
    CheckUrlCallback callback_;

    // Used in the native call case.
    NativeCheckUrlCallback native_callback_;
  };

  struct KickOffLookupMechanismResult {
    KickOffLookupMechanismResult(
        SafeBrowsingLookupMechanism::StartCheckResult start_check_result,
        PerformedCheck performed_check);
    ~KickOffLookupMechanismResult();

    SafeBrowsingLookupMechanism::StartCheckResult start_check_result;
    PerformedCheck performed_check;
  };

  // Called once the lookup mechanism runner indicates that the check is
  // complete. |performed_check| indicates what kind of check was performed. The
  // |result| parameter will only be populated if |timed_out| is false. This
  // function eventually decides whether or not to show a blocking page.
  void OnUrlResultAndMaybeDeleteSelf(
      PerformedCheck performed_check,
      bool timed_out,
      std::optional<std::unique_ptr<CompleteCheckResult>> result);

  // Helper function to handle deciding whether or not to show a blocking page.
  void OnUrlResultInternalAndMaybeDeleteSelf(
      const GURL& url,
      SBThreatType threat_type,
      const ThreatMetadata& metadata,
      std::optional<ThreatSource> threat_source,
      std::unique_ptr<RTLookupResponse> rt_lookup_response,
      bool timed_out,
      PerformedCheck performed_check);

  void CheckUrlImplAndMaybeDeleteSelf(const GURL& url,
                                      const std::string& method,
                                      Notifier notifier);

  // NOTE: this method runs callbacks which could destroy this object.
  void ProcessUrlsAndMaybeDeleteSelf();

  // NOTE: this method runs callbacks which could destroy this object.
  void BlockAndProcessUrlsAndMaybeDeleteSelf(
      bool showed_interstitial,
      bool has_post_commit_interstitial_skipped,
      PerformedCheck performed_check);

  void OnBlockingPageCompleteAndMaybeDeleteSelf(
      PerformedCheck performed_check,
      security_interstitials::UnsafeResource::UrlCheckResult result);

  // Helper method that checks whether |url|'s reputation can be checked using
  // real time lookups.
  bool CanPerformFullURLLookup(const GURL& url);

  // Helper method to get the correct type of hash look up mechanism.
  std::unique_ptr<SafeBrowsingLookupMechanism> GetHashRealTimeLookupMechanism(
      const GURL& url,
      bool can_use_hash_real_time_service,
      bool can_use_hash_real_time_db_manager);

  // This will decide which mechanism to use for a lookup and then perform it.
  KickOffLookupMechanismResult KickOffLookupMechanism(const GURL& url);

  SBThreatType CheckWebUIUrls(const GURL& url);

  // Returns false if this object has been destroyed by the callback. In that
  // case none of the members of this object should be touched again.
  bool RunNextCallbackAndMaybeDeleteSelf(
      bool proceed,
      bool showed_interstitial,
      bool has_post_commit_interstitial_skipped,
      PerformedCheck performed_check);

  security_interstitials::UnsafeResource MakeUnsafeResource(
      const GURL& url,
      SBThreatType threat_type,
      const ThreatMetadata& metadata,
      ThreatSource threat_source,
      std::unique_ptr<RTLookupResponse> rt_lookup_response,
      PerformedCheck performed_check);

  enum State {
    // Haven't started checking or checking is complete.
    STATE_NONE,
    // We have one outstanding URL-check.
    STATE_CHECKING_URL,
    // A warning must be shown, but it's delayed because of the Delayed Warnings
    // experiment.
    STATE_DELAYED_BLOCKING_PAGE,
    // We're displaying a blocking page.
    STATE_DISPLAYING_BLOCKING_PAGE,
    // The blocking page has returned *not* to proceed.
    STATE_BLOCKED
  };

  struct UrlInfo {
    UrlInfo(const GURL& url, const std::string& method, Notifier notifier);
    UrlInfo(UrlInfo&& other);

    ~UrlInfo();

    GURL url;
    std::string method;
    Notifier notifier;
  };

  SEQUENCE_CHECKER(sequence_checker_);
  const net::HttpRequestHeaders headers_;
  const int load_flags_;
  const bool has_user_gesture_;
  // TODO(https://crbug.com/40683815): |weak_web_state_| is only used on iOS,
  // and |web_contents_getter_|, |render_process_id_|, |render_frame_token_|,
  // and |frame_tree_node_id_| are used on all other platforms.  This class
  // should be refactored to use only the common functionality can be shared
  // across platforms. Note that this blocks the refactoring of
  // components/security_interstitials, https://crbug.com/40686246.
  base::RepeatingCallback<content::WebContents*()> web_contents_getter_;
  const security_interstitials::UnsafeResource::RenderProcessId
      render_process_id_ =
          security_interstitials::UnsafeResource::kNoRenderProcessId;
  const security_interstitials::UnsafeResource::RenderFrameToken
      render_frame_token_;
  const security_interstitials::UnsafeResource::FrameTreeNodeId
      frame_tree_node_id_ =
          security_interstitials::UnsafeResource::kNoFrameTreeNodeId;
  const std::optional<int64_t> navigation_id_;
  base::WeakPtr<web::WebState> weak_web_state_;
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // The redirect chain for this resource, including the original URL and
  // subsequent redirect URLs.
  std::vector<UrlInfo> urls_;
  // |urls_| before |next_index_| have been checked. If |next_index_| is smaller
  // than the size of |urls_|, the URL at |next_index_| is being processed.
  size_t next_index_ = 0;

  State state_ = STATE_NONE;

  // Whether real time URL lookup is enabled for this request.
  bool url_real_time_lookup_enabled_;

  // Whether safe browsing database can be checked. It is set to false when
  // enterprise real time URL lookup is enabled and safe browsing is disabled
  // for this profile.
  bool can_check_db_;

  // Whether the high confidence allowlist can be checked. It is set to false
  // when enterprise real time URL lookup is enabled.
  bool can_check_high_confidence_allowlist_ = true;

  // URL Lookup service suffix for logging metrics.
  std::string url_lookup_service_metric_suffix_;

  // The task runner for the UI thread.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // This object is used to perform real time url check. Can only be accessed in
  // UI thread.
  base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui_;

  // This object is used to perform the hash-prefix real-time lookup. It can
  // only be accessed on the UI thread.
  base::WeakPtr<HashRealTimeService> hash_realtime_service_on_ui_;

  // This is created when a specific mechanism check needs to be run, and is
  // destroyed once the check completes. This handles running the check and
  // responding back to this class once the check is complete or has timed out.
  std::unique_ptr<SafeBrowsingLookupMechanismRunner> lookup_mechanism_runner_;

  // What kind of hash-prefix real-time lookup is enabled for this request, if
  // any.
  hash_realtime_utils::HashRealTimeSelection hash_realtime_selection_ =
      hash_realtime_utils::HashRealTimeSelection::kNone;

  // If this check allows navigation to commit before it completes.
  const bool is_async_check_;

  // If the allowlist should be checked before the hash database check. This
  // is useful to speed up the check if the allowlist check is faster than the
  // hash database check. Callers should only set this value to true if they
  // fully trust the correctness of the allowlist or there are other mitigations
  // in place when blocklisted URLs are mistakenly added in the allowlist.
  bool check_allowlist_before_hash_database_ = false;

  // The current tab ID. Used sometimes for identifying the referrer chain for
  // URL real-time lookups. Can be |SessionID::InvalidValue()|.
  SessionID tab_id_;

  base::WeakPtrFactory<SafeBrowsingUrlCheckerImpl> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_
