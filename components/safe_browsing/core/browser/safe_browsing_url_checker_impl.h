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
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_experimenter.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_runner.h"
#include "components/safe_browsing/core/browser/url_realtime_mechanism.h"
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
  using NativeUrlCheckNotifier =
      base::OnceCallback<void(bool /* proceed */,
                              bool /* showed_interstitial */,
                              bool /* did_perform_url_real_time_check */,
                              bool /* did_check_url_real_time_allowlist */)>;

  // If |slow_check_notifier| is not null, the callback is supposed to update
  // this output parameter with a callback to receive complete notification. In
  // that case, |proceed| and |showed_interstitial| should be ignored.
  using NativeCheckUrlCallback =
      base::OnceCallback<void(NativeUrlCheckNotifier* /* slow_check_notifier */,
                              bool /* proceed */,
                              bool /* showed_interstitial */,
                              bool /* did_perform_url_real_time_check */,
                              bool /* did_check_url_real_time_allowlist */)>;

  // Constructor for SafeBrowsingUrlCheckerImpl. |url_real_time_lookup_enabled|
  // indicates whether or not the profile has enabled real time URL lookups, as
  // computed by the RealTimePolicyEngine. |hash_realtime_selection| indicates
  // which type of hash-prefix real-time lookup the profile is eligible for, if
  // any. These two must be computed in advance, since this class only exists
  // on the IO thread.
  // |can_urt_check_subresource_url| indicates whether or not the profile has
  // enabled real time URL lookups for subresource URLs. If this value is true,
  // then |url_real_time_lookup_enabled| must also be true.
  // |last_committed_url| is used for obtaining the page load token when the
  // URL being checked is not a mainframe URL. Only used when URL real time
  // lookup is performed.
  // |webui_delegate_| is allowed to be null. If non-null, it must outlive this
  // object.
  SafeBrowsingUrlCheckerImpl(
      const net::HttpRequestHeaders& headers,
      int load_flags,
      network::mojom::RequestDestination request_destination,
      bool has_user_gesture,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      security_interstitials::UnsafeResource::RenderProcessId render_process_id,
      security_interstitials::UnsafeResource::RenderFrameId render_frame_id,
      security_interstitials::UnsafeResource::FrameTreeNodeId
          frame_tree_node_id,
      bool url_real_time_lookup_enabled,
      bool can_urt_check_subresource_url,
      bool can_check_db,
      bool can_check_high_confidence_allowlist,
      std::string url_lookup_service_metric_suffix,
      GURL last_committed_url,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      UrlRealTimeMechanism::WebUIDelegate* webui_delegate,
      base::WeakPtr<HashRealTimeService> hash_realtime_service_on_ui,
      scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
          mechanism_experimenter,
      bool is_mechanism_experiment_allowed,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection);

  // Constructor that takes only a RequestDestination, a UrlCheckerDelegate, and
  // real-time lookup-related arguments, omitting other arguments that never
  // have non-default values on iOS.
  // TODO(crbug.com/1103222): Add an iOS-specific WebUIDelegate implementation
  // and pass it here to log URT requests/responses on open
  // chrome://safe-browsing pages once chrome://safe-browsing works on iOS, or
  // else to log those requests/responses to stderr.
  SafeBrowsingUrlCheckerImpl(
      network::mojom::RequestDestination request_destination,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      base::WeakPtr<web::WebState> weak_web_state,
      bool url_real_time_lookup_enabled,
      bool can_urt_check_subresource_url,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui);

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

 private:
  using CompleteCheckResult = SafeBrowsingLookupMechanism::CompleteCheckResult;

  class Notifier {
   public:
    explicit Notifier(CheckUrlCallback callback);
    explicit Notifier(NativeCheckUrlCallback native_callback);

    ~Notifier();

    Notifier(Notifier&& other);
    Notifier& operator=(Notifier&& other);

    void OnStartSlowCheck();
    void OnCompleteCheck(bool proceed,
                         bool showed_interstitial,
                         bool did_perform_url_real_time_check,
                         bool did_check_url_real_time_allowlist);

   private:
    // Used in the mojo interface case.
    CheckUrlCallback callback_;
    mojo::Remote<mojom::UrlCheckNotifier> slow_check_notifier_;

    // Used in the native call case.
    NativeCheckUrlCallback native_callback_;
    NativeUrlCheckNotifier native_slow_check_notifier_;
  };

  // Called once the lookup mechanism runner indicates that the check is
  // complete. The |result| parameter will only be populated if |timed_out| is
  // false. This function eventually decides whether or not to show a blocking
  // page.
  void OnUrlResultAndMaybeDeleteSelf(
      bool timed_out,
      absl::optional<std::unique_ptr<CompleteCheckResult>> result);

  // Helper function to handle deciding whether or not to show a blocking page.
  void OnUrlResultInternalAndMaybeDeleteSelf(
      const GURL& url,
      SBThreatType threat_type,
      const ThreatMetadata& metadata,
      absl::optional<ThreatSource> threat_source,
      std::unique_ptr<RTLookupResponse> rt_lookup_response,
      bool timed_out);

  void CheckUrlImplAndMaybeDeleteSelf(const GURL& url,
                                      const std::string& method,
                                      Notifier notifier);

  // NOTE: this method runs callbacks which could destroy this object.
  void ProcessUrlsAndMaybeDeleteSelf();

  // NOTE: this method runs callbacks which could destroy this object.
  void BlockAndProcessUrlsAndMaybeDeleteSelf(bool showed_interstitial);

  void OnBlockingPageCompleteAndMaybeDeleteSelf(bool proceed,
                                                bool showed_interstitial);

  // Helper method that checks whether |url|'s reputation can be checked using
  // real time lookups.
  bool CanPerformFullURLLookup(const GURL& url);

  // This will decide which mechanism to use for a lookup and then perform it.
  // This may include running a SafeBrowsingLookupMechanism experiment if
  // eligible.
  SafeBrowsingLookupMechanism::StartCheckResult KickOffLookupMechanism(
      const GURL& url,
      bool* out_hash_database_check_was_performed);

  SBThreatType CheckWebUIUrls(const GURL& url);

  // Returns false if this object has been destroyed by the callback. In that
  // case none of the members of this object should be touched again.
  bool RunNextCallbackAndMaybeDeleteSelf(bool proceed,
                                         bool showed_interstitial);

  security_interstitials::UnsafeResource MakeUnsafeResource(
      const GURL& url,
      SBThreatType threat_type,
      const ThreatMetadata& metadata,
      ThreatSource threat_source,
      std::unique_ptr<RTLookupResponse> rt_lookup_response);

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
    UrlInfo(const GURL& url,
            const std::string& method,
            Notifier notifier,
            bool did_perform_url_real_time_check,
            bool did_check_url_real_time_allowlist);
    UrlInfo(UrlInfo&& other);

    ~UrlInfo();

    GURL url;
    std::string method;
    Notifier notifier;

    // Whether real time URL check (including allowlist and cache checks) was
    // performed.
    bool did_perform_url_real_time_check;
    // If the allowlist was checked for this URL.
    bool did_check_url_real_time_allowlist;
  };

  SEQUENCE_CHECKER(sequence_checker_);
  const net::HttpRequestHeaders headers_;
  const int load_flags_;
  const network::mojom::RequestDestination request_destination_;
  const bool has_user_gesture_;
  // TODO(crbug.com/1069047): |weak_web_state_| is only used on iOS, and
  // |web_contents_getter_|, |render_process_id_|, |render_frame_id_|, and
  // |frame_tree_node_id_| are used on all other platforms.  This class should
  // be refactored to use only the common functionality can be shared across
  // platforms.
  base::RepeatingCallback<content::WebContents*()> web_contents_getter_;
  const security_interstitials::UnsafeResource::RenderProcessId
      render_process_id_ =
          security_interstitials::UnsafeResource::kNoRenderProcessId;
  const security_interstitials::UnsafeResource::RenderFrameId render_frame_id_ =
      security_interstitials::UnsafeResource::kNoRenderFrameId;
  const security_interstitials::UnsafeResource::FrameTreeNodeId
      frame_tree_node_id_ =
          security_interstitials::UnsafeResource::kNoFrameTreeNodeId;
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

  // Whether non mainframe url can be checked for this profile.
  bool can_urt_check_subresource_url_;

  // Whether safe browsing database can be checked. It is set to false when
  // enterprise real time URL lookup is enabled and safe browsing is disabled
  // for this profile.
  bool can_check_db_;

  // Whether the high confidence allowlist can be checked. It is set to false
  // when enterprise real time URL lookup is enabled.
  bool can_check_high_confidence_allowlist_ = true;

  // URL Lookup service suffix for logging metrics.
  std::string url_lookup_service_metric_suffix_;

  // The last committed URL when the checker is constructed. It is used to
  // obtain page load token when the URL being checked is not a mainframe URL.
  // Only used when real time lookup is performed.
  GURL last_committed_url_;

  // The task runner for the UI thread.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // This object is used to perform real time url check. Can only be accessed in
  // UI thread.
  base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui_;

  // May be null on certain platforms that don't support chrome://safe-browsing
  // and in unit tests. If non-null, guaranteed to outlive this object by
  // contract.
  raw_ptr<UrlRealTimeMechanism::WebUIDelegate, LeakedDanglingUntriaged>
      webui_delegate_ = nullptr;

  // This object is used to perform the hash-prefix real-time lookup. It can
  // only be accessed on the UI thread.
  base::WeakPtr<HashRealTimeService> hash_realtime_service_on_ui_;

  // This is created when a specific mechanism check needs to be run, and is
  // destroyed once the check completes. This handles running the check and
  // responding back to this class once the check is complete or has timed out.
  std::unique_ptr<SafeBrowsingLookupMechanismRunner> lookup_mechanism_runner_;

  // If eligible, this class will run a SafeBrowsingLookupMechanism experiment
  // instead of just running a real-time URL check. It will check if the
  // experiment is eligible and then perform it through this object. This can
  // only be populated if |is_mechanism_experiment_allowed_| = true, though it
  // may not be populated even if it is true (if the URL is non-mainframe).
  scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
      mechanism_experimenter_;

  // When true, instructs the V4 protocol manager to keep multiple separate
  // copies of the cache for use by the experiment. See comments defined above
  // MechanismExperimentHashDatabaseCache for more details.
  bool is_mechanism_experiment_allowed_ = false;

  // What kind of hash-prefix real-time lookup is enabled for this request, if
  // any.
  hash_realtime_utils::HashRealTimeSelection hash_realtime_selection_ =
      hash_realtime_utils::HashRealTimeSelection::kNone;

  base::WeakPtrFactory<SafeBrowsingUrlCheckerImpl> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SAFE_BROWSING_URL_CHECKER_IMPL_H_
