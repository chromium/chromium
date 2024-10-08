// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_HOST_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_HOST_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/permissions/permission_request_manager.h"
#include "components/safe_browsing/content/browser/async_check_tracker.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "url/gurl.h"

namespace base {
class TickClock;
}

namespace safe_browsing {
class ClientPhishingRequest;
class ClientSideDetectionService;

// This class is used to receive the IPC from the renderer which
// notifies the browser that a URL was classified as phishing.  This
// class relays this information to the client-side detection service
// class which sends a ping to a server to validate the verdict.
class ClientSideDetectionHost
    : public content::WebContentsObserver,
      public permissions::PermissionRequestManager::Observer,
      public AsyncCheckTracker::Observer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AsyncCheckTriggerForceRequestResult {
    kTriggered = 0,
    kSkippedTriggerModelsPingNotSkipped = 1,
    kSkippedNotForced = 2,
    kMaxValue = kSkippedNotForced,
  };

  // A callback via which the client of this component indicates whether the
  // primary account is signed in.
  using PrimaryAccountSignedIn = base::RepeatingCallback<bool()>;

  // Delegate which allows to provide embedder specific implementations.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns whether there is a SafeBrowsingUserInteractionObserver available.
    virtual bool HasSafeBrowsingUserInteractionObserver() = 0;
    // Returns the prefs service associated with the current embedders profile.
    virtual PrefService* GetPrefs() = 0;
    virtual scoped_refptr<SafeBrowsingDatabaseManager>
    GetSafeBrowsingDBManager() = 0;
    virtual scoped_refptr<BaseUIManager> GetSafeBrowsingUIManager() = 0;
    virtual base::WeakPtr<ClientSideDetectionService>
    GetClientSideDetectionService() = 0;
    virtual void AddReferrerChain(ClientPhishingRequest* verdict,
                                  GURL current_url,
                                  const content::GlobalRenderFrameHostId&
                                      current_outermost_main_frame_id) = 0;
    virtual VerdictCacheManager* GetCacheManager() = 0;
    // Returns the management status for current profile.
    virtual ChromeUserPopulation GetUserPopulation() = 0;
  };

  // The caller keeps ownership of the tab object and is responsible for
  // ensuring that it stays valid until WebContentsDestroyed is called.
  // The caller also keeps ownership of pref_service. The
  // ClientSideDetectionHost takes ownership of token_fetcher. is_off_the_record
  // indicates if the profile is incognito, and account_signed_in_callback is
  // checked to find out if primary account is signed in.
  static std::unique_ptr<ClientSideDetectionHost> Create(
      content::WebContents* tab,
      std::unique_ptr<Delegate> delegate,
      PrefService* pref_service,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      bool is_off_the_record,
      const PrimaryAccountSignedIn& account_signed_in_callback);

  ClientSideDetectionHost(const ClientSideDetectionHost&) = delete;
  ClientSideDetectionHost& operator=(const ClientSideDetectionHost&) = delete;

  ~ClientSideDetectionHost() override;

  // From content::WebContentsObserver.  If we navigate away we cancel all
  // pending callbacks that could show an interstitial, and check to see whether
  // we should classify the new URL. If a request to lock the keyboard or
  // pointer or vibrate the page has arrived, we will re-trigger classification.
  void PrimaryPageChanged(content::Page& page) override;
  void KeyboardLockRequested() override;
  void PointerLockRequested() override;
  void VibrationRequested() override;

  // permissions::PermissionRequestManager::Observer methods:
  void OnPromptAdded() override;
  void OnPermissionRequestManagerDestructed() override;

  void RegisterPermissionRequestManager();

  // AsyncCheckTracker::Observer methods:
  void OnAsyncSafeBrowsingCheckCompleted() override;
  void OnAsyncSafeBrowsingCheckTrackerDestructed() override;

  void RegisterAsyncCheckTracker();

 protected:
  explicit ClientSideDetectionHost(
      content::WebContents* tab,
      std::unique_ptr<Delegate> delegate,
      PrefService* pref_service,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      bool is_off_the_record,
      const PrimaryAccountSignedIn& account_signed_in_callback);

  // Used for testing.
  void set_ui_manager(BaseUIManager* ui_manager);
  void set_database_manager(SafeBrowsingDatabaseManager* database_manager);

 private:
  friend class ClientSideDetectionHostTestBase;
  friend class ClientSideDetectionHostNotificationTest;
  class ShouldClassifyUrlRequest;
  friend class ShouldClassifyUrlRequest;
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostPrerenderBrowserTest,
                           PrerenderShouldNotAffectClientSideDetection);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostPrerenderBrowserTest,
                           ClassifyPrerenderedPageAfterActivation);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderBrowserTest,
      ClassifyPrerenderedPageAfterActivationAndCheckDebuggingMetadataCache);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderBrowserTest,
      CheckDebuggingMetadataCacheAfterClearingCacheAfterNavigation);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest,
      KeyboardLockTriggersPreclassificationCheck);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest,
      PointerLockTriggersPreClassificationCheck);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest,
      PointerLockClassificationTriggersCSPPPing);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostPrerenderExclusiveAccessBrowserTest,
      KeyboardLockClassificationTriggersCSPPPing);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostVibrateTest,
                           VibrationApiTriggersPreclassificationCheck);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionHostVibrateTest,
                           VibrationApiClassificationTriggersCSPPPing);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostTest,
      TestPreClassificationCheckMatchHighConfidenceAllowlist);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostTest,
      TestPreClassificationCheckDoesNotMatchHighConfidenceAllowlist);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionHostTest,
      TestPreClassificationCheckDoesNotMatchHighConfidenceAllowlistDueToDisabledFeature);
  FRIEND_TEST_ALL_PREFIXES(
      ClientSideDetectionRTLookupResponseForceRequestTest,
      AsyncCheckTrackerTriggersClassificationRequestOnAllowlistMatch);

  // Helper function to create preclassification check once requirements are
  // met.
  void MaybeStartPreClassification(ClientSideDetectionType request_type);

  // Called when pre-classification checks are done for the phishing
  // classifiers. |request_type| is passed in to specify the process that
  // requests the classification.
  void OnPhishingPreClassificationDone(
      ClientSideDetectionType request_type,
      bool should_classify,
      bool is_sample_ping,
      std::optional<bool> did_match_high_confidence_allowlist);

  // `verdict` is a wrapped ClientPhishingRequest protocol message, `result`
  // is the outcome of the renderer classification. `request_type` is passed in
  // to specify the process that requests the classification, which is passed
  // along from OnPhishingPreClassificationDone().
  void PhishingDetectionDone(
      ClientSideDetectionType request_type,
      bool is_sample_ping,
      std::optional<bool> did_match_high_confidence_allowlist,
      mojom::PhishingDetectorResult result,
      std::optional<mojo_base::ProtoWrapper> verdict);

  // `verdict` is the ClientPhishingRequest passed into PhishingDetectionDone().
  void MaybeSendClientPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist);

  // |verdict| is an encoded ClientPhishingRequest protocol message, |result| is
  // the outcome of the renderer image embedding. The verdict is passed into
  // this function after the renderer classification is finished.
  void PhishingImageEmbeddingDone(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      mojom::PhishingImageEmbeddingResult result,
      std::optional<mojo_base::ProtoWrapper> image_feature_embedding);

  // Callback that is called when the server ping back is
  // done. Display an interstitial if |is_phishing| is true.
  // Otherwise, we do nothing. Called in UI thread. |is_from_cache| indicates
  // whether the warning is being shown due to a cached verdict or from an
  // actual server ping. |response_code| is cached so it can be included as
  // debugging metadata in PhishGuard pings.
  void MaybeShowPhishingWarning(
      bool is_from_cache,
      ClientSideDetectionType request_type,
      std::optional<bool> did_match_high_confidence_allowlist,
      GURL phishing_url,
      bool is_phishing,
      std::optional<net::HttpStatusCode> response_code);

  // Whether request is forced for |current_url_|. This function also checks
  // whether enhanced protection is enabled.
  bool HasForceRequestFromRtUrlLookup();

  // Used for testing.  This function does not take ownership of the service
  // class.
  void set_client_side_detection_service(
      base::WeakPtr<ClientSideDetectionService> service);

  // Sets a test tick clock only for testing.
  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

  // Sets the token fetcher only for testing.
  void set_token_fetcher_for_testing(
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher) {
    token_fetcher_ = std::move(token_fetcher);
  }

  // Sets the incognito bit only for testing.
  void set_is_off_the_record_for_testing(bool is_off_the_record) {
    is_off_the_record_ = is_off_the_record;
  }

  // Sets the primary account signed in callback for testing.
  void set_account_signed_in_for_testing(
      const PrimaryAccountSignedIn& account_signed_in_callback) {
    account_signed_in_callback_ = account_signed_in_callback;
  }

  void set_high_confidence_allowlist_acceptance_rate_for_testing(
      float acceptance_rate);

  // Check if CSD can get an access Token. Should be enabled only for ESB
  // users, who are signed in and not in incognito mode.
  bool CanGetAccessToken();

  // Send the client report to CSD server.
  void SendRequest(std::unique_ptr<ClientPhishingRequest> verdict,
                   const std::string& access_token,
                   std::optional<bool> did_match_high_confidence_allowlist);

  // Called when token_fetcher_ has fetched the token.
  void OnGotAccessToken(std::unique_ptr<ClientPhishingRequest> verdict,
                        std::optional<bool> did_match_high_confidence_allowlist,
                        const std::string& access_token);

  // Check if sample ping can be sent to Safe Browsing.
  bool CanSendSamplePing();

  // This pointer may be nullptr if client-side phishing detection is
  // disabled.
  base::WeakPtr<ClientSideDetectionService> csd_service_;
  // The WebContents that the class is observing.
  raw_ptr<content::WebContents> tab_;
  // These pointers may be nullptr if SafeBrowsing is disabled.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<BaseUIManager> ui_manager_;
  // Keep a handle to the latest classification request so that we can cancel
  // it if necessary.
  std::unique_ptr<ShouldClassifyUrlRequest> classification_request_;
  // The current URL
  GURL current_url_;
  // The current outermost main frame's id.
  content::GlobalRenderFrameHostId current_outermost_main_frame_id_;

  // Records the start time of when phishing detection started.
  base::TimeTicks phishing_detection_start_time_;
  raw_ptr<const base::TickClock> tick_clock_;

  std::unique_ptr<Delegate> delegate_;

  // Unowned object used for getting preference settings.
  raw_ptr<PrefService> pref_service_;

  // The token fetcher used for getting access token.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  // A boolean indicates whether the associated profile associated is an
  // incognito profile.
  bool is_off_the_record_;

  // Callback for checking if the user is signed in, before fetching
  // acces_token.
  PrimaryAccountSignedIn account_signed_in_callback_;

  // The remote for the currently active phishing classification.
  mojo::AssociatedRemote<mojom::PhishingDetector> phishing_detector_;

  // The remote for the currently active phishing image embedder.
  mojo::AssociatedRemote<mojom::PhishingImageEmbedderDetector>
      phishing_image_embedder_;

  base::ScopedObservation<permissions::PermissionRequestManager,
                          permissions::PermissionRequestManager::Observer>
      permission_request_observation_{this};

  // A boolean indicates whether TRIGGER_MODELS request is skipped. This is
  // used to decide whether async check is allowed to trigger FORCE_REQUEST.
  bool trigger_models_request_skipped_ = false;

  // Modified through tests only. Initial value is set to the const
  // kProbabilityForAcceptingHCAllowlistTrigger.
  float probability_for_accepting_hc_allowlist_trigger_;

  base::ScopedObservation<AsyncCheckTracker, AsyncCheckTracker::Observer>
      async_check_observation_{this};

  base::WeakPtrFactory<ClientSideDetectionHost> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_HOST_H_
