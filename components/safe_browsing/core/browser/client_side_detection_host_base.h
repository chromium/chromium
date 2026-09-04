// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_HOST_BASE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_HOST_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/safe_browsing/core/browser/credit_card_form_event.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/client_side_detection_enums.h"
#include "components/safe_browsing/core/common/phishing_classifier/phishing_image_embedder.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/threat_enums.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

class PrefService;

namespace history {
class HistoryService;
}

namespace safe_browsing {

using HostInnerTextCallback = base::OnceCallback<void(std::string)>;

std::string_view GetRequestTypeName(
    ClientSideDetectionType client_side_detection_type);

// Probability value used to sample pings on CSD allowlist match. For other safe
// browsing countermeasures, we sample at 1 in 100 rate, but in this, we hit the
// allowlist 1000 times more than the rate at which we send a ping due to local
// model verdict. Therefore, we sample at 1 in 100,000 rate instead.
inline constexpr float kProbabilityForSendingSampleRequest = 0.000001f;
// Probability value used to accept the high confidence allowlist match for
// trigger and force request types. More information on why this value was
// chosen can be found at go/crca-cspp-expand-allowlist.
inline constexpr float kProbabilityForAcceptingHCAllowlistTrigger = 0.9999f;

class ClientSideDetectionFeatureCacheBase;
class ClientSideDetectionServiceBase;
class VerdictCacheManager;

class ClientSideDetectionHostBase : public autofill::AutofillManager::Observer,
                                    public history::HistoryServiceObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ImageEmbeddingResult)
  enum class ImageEmbeddingResult {
    kSuccess = 0,
    kImageEmbedderNotReady = 1,
    kCancelled = 2,
    kForwardBackTransition = 3,
    kFailed = 4,
    kInvalidURLFormatRequest = 5,
    kInvalidDocumentLoader = 6,
    kMaxValue = kInvalidDocumentLoader,
  };
  // LINT.ThenChange(//components/safe_browsing/content/common/safe_browsing.mojom:PhishingImageEmbeddingResult)

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AsyncCheckTriggerForceRequestResult {
    kTriggered = 0,
    kSkippedTriggerModelsPingNotSkipped = 1,  // DEPRECATED
    kSkippedNotForced = 2,
    kSkippedTriggerModelsPingSentAsForceRequest = 3,
    kMaxValue = kSkippedTriggerModelsPingSentAsForceRequest,
  };

  ClientSideDetectionHostBase(
      base::WeakPtr<ClientSideDetectionServiceBase> csd_service,
      VerdictCacheManager* cache_manager,
      IntelligentScanDelegate* intelligent_scan_delegate,
      PrefService* pref_service,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      history::HistoryService* history_service,
      bool is_off_the_record);
  ~ClientSideDetectionHostBase() override;

  ClientSideDetectionHostBase(const ClientSideDetectionHostBase&) = delete;
  ClientSideDetectionHostBase& operator=(const ClientSideDetectionHostBase&) =
      delete;

  virtual GURL GetCurrentUrl() const = 0;
  virtual ClientSideDetectionFeatureCacheBase* GetFeatureCache() = 0;
  virtual std::vector<GURL> GetRedirectChain() = 0;
  virtual credit_card_form::ReferringApp GetReferringApp() const = 0;
  virtual ChromeUserPopulation GetUserPopulation() = 0;

  virtual bool IsAccountSignedIn() = 0;
  virtual bool IsErrorDocument() = 0;

  // Returns the site engagement score for `url`. Returns std::nullopt if the
  // score is not available (e.g., for off-the-record profiles, if WebContents
  // is null, or if SiteEngagementService is unavailable).
  virtual std::optional<double> GetSiteEngagementScore(const GURL& url) const;

  // Returns the inner text from the tab. The callback is used to retrieve a
  // string back when the inner text function is completed. This string is then
  // used to provide the intelligent scan delegate the information about the
  // page.
  virtual void GetInnerText(HostInnerTextCallback callback) = 0;

  // Calls the CSD service to classify phishing through thresholds presented in
  // `verdict`.
  virtual void ClassifyPhishingThroughThresholds(
      ClientPhishingRequest* verdict);

  // Called by `MaybeSendClientPhishingRequest` to determine whether to perform
  // image embedding as part of the client-side phishing detection flow.
  //
  // Because image embedding relies on platform/renderer-specific APIs (such as
  // RenderFrameHost/Mojo on desktop/Android, or SnapshotTabHelper on iOS), the
  // implementation is delegated to the derived class.
  //
  // Derived classes implementing this method MUST:
  // 1. Determine whether visual features can be extracted (e.g. by checking
  //    user opt-ins, incognito status, and viewport size limits using
  //    `visual_utils::CanExtractVisualFeatures`).
  // 2. Clear the visual features image from the verdict
  //    (`verdict->mutable_visual_features()->clear_image()`) if the check
  //    determines that extraction is not allowed.
  // 3. Initiate the image embedding process (if allowed), forwarding the result
  //    to `MaybeStartIntelligentScanForScamDetection` upon completion (or
  //    immediately, if image embedding is skipped).
  //
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionVibrationApi and kClientSideDetectionKeyboardLock are
  // removed.
  virtual void MaybeStartImageEmbedding(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      PhishingDetectorResult result) = 0;

  // Helper method to run the callback.
  virtual void MaybeRunUserReportCallback() = 0;

  // Triggers Gemini Antiscam Protection if conditions are met.
  virtual void MaybeStartGeminiAntiscamProtection(
      GURL url,
      ClientSideDetectionType request_type,
      std::optional<bool> did_match_high_confidence_allowlist) = 0;

  // Helper function to create preclassification check once requirements are
  // met.
  virtual void MaybeStartPreClassification(
      ClientSideDetectionType request_type) = 0;

  // Called when an asynchronous Safe Browsing URL check completes.
  void OnAsyncSafeBrowsingCheckCompleted();

  // history::HistoryServiceObserver method:
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // autofill::AutofillManager::Observer methods:
  void OnAfterFocusOnFormField(autofill::AutofillManager& manager,
                               autofill::FormGlobalId form_id,
                               autofill::FieldGlobalId field_id) override;
  void OnFieldTypesDetermined(autofill::AutofillManager& manager,
                              autofill::FormGlobalId form,
                              FieldTypeSource source,
                              bool small_forms_were_parsed) override;

  void SetAndObserveHistoryServiceForTesting(history::HistoryService* service);

  // Used for testing.  This function does not take ownership of the service
  // class.
  void set_client_side_detection_service_for_testing(
      base::WeakPtr<ClientSideDetectionServiceBase> service) {
    csd_service_ = service;
  }

  void set_token_fetcher_for_testing(
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher) {
    token_fetcher_ = std::move(token_fetcher);
  }

  void set_intelligent_scan_delegate_for_testing(
      IntelligentScanDelegate* intelligent_scan_delegate) {
    intelligent_scan_delegate_ = intelligent_scan_delegate;
  }

  bool is_off_the_record() const { return is_off_the_record_; }

  void set_is_off_the_record_for_testing(bool is_off_the_record) {
    is_off_the_record_ = is_off_the_record;
  }

  // Sets a test tick clock only for testing.
  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

  // Overrides the high-confidence allowlist acceptance rate for testing.
  void set_high_confidence_allowlist_acceptance_rate_for_testing(
      float acceptance_rate) {
    probability_for_accepting_hc_allowlist_trigger_ = acceptance_rate;
  }

  // Overrides the sample ping rate for testing.
  void set_sample_ping_rate_for_testing(float sample_ping_rate) {
    probability_for_sending_sample_request_ = sample_ping_rate;
  }

 protected:
  base::WeakPtr<ClientSideDetectionServiceBase> GetClientSideDetectionService()
      const;
  IntelligentScanDelegate* GetIntelligentScanDelegate() const;
  PrefService* GetPrefs() const;

  std::optional<base::UnguessableToken> GetIntelligentScanId() const;
  bool IsEnhancedProtectionEnabled() const;

  static safe_browsing::ThreatSubtype GetThreatSubtype(
      IntelligentScanVerdict intelligent_scan_verdict);

  // Cancels any pending asynchronous requests bound to this host.
  // Intended to handle the case where the primary page changes while there is
  // a pending phishing report request. We have to cancel it to make sure we
  // don't display an interstitial for the wrong page. Note that this won't
  // cancel the server ping back but only cancel the showing of the
  // interstitial.
  virtual void CancelPendingRequests();

  // OnCreditCardFormVisitCount is a callback that is called when site
  // visit count on a credit card form event is complete, at which point
  // it determines whether a credit card from event should trigger a CSD
  // ping.
  void OnCreditCardFormVisitCount(
      std::optional<base::TimeTicks> start_time,
      credit_card_form::FieldDetectionHeuristic field_heuristic,
      std::string event_name,
      bool should_trigger,
      history::DailyVisitsResult history_result);

  // `verdict` is a wrapped ClientPhishingRequest protocol message, `result`
  // is the outcome of the renderer classification. `request_type` is passed in
  // to specify the process that requests the classification, which is passed
  // along from OnPhishingPreClassificationDone().
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  void PhishingDetectionDone(
      ClientSideDetectionType request_type,
      bool is_sample_ping,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      base::TimeTicks start_time,
      PhishingDetectorResult result,
      std::optional<ClientPhishingRequest> verdict);

  // `verdict` is the ClientPhishingRequest passed into PhishingDetectionDone().
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  void MaybeSendClientPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      PhishingDetectorResult result);

  // |verdict| is an encoded ClientPhishingRequest protocol message, |result| is
  // the outcome of the image embedding. The verdict is passed into
  // this function after the renderer classification is finished.
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  void PhishingImageEmbeddingDone(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      ImageEmbeddingResult result,
      std::optional<ImageFeatureEmbedding> image_feature_embedding,
      std::optional<VisualFeatures> visual_features);

  // `verdict` is an encoded ClientPhishingRequest protocol message, which will
  // contain the intelligent scan result if the execution is successful.
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  void MaybeStartIntelligentScanForScamDetection(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip);

  // Callback function when GetInnerText is completed in the delegate. This
  // inner text is fetched as part of intelligent scan through the
  // CSD service class.
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  void OnInnerTextComplete(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      std::string inner_text);

  // Callback function when StartIntelligentScan from the intelligent scan
  // delegate is completed.
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  void OnIntelligentScanDone(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      IntelligentScanDelegate::IntelligentScanResult response);

  // `verdict` is an encoded ClientPhishingRequest protocol message. This is the
  // last step before sending the ping to the server.
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  void MaybeGetAccessToken(
      std::unique_ptr<ClientPhishingRequest> verdict,
      std::optional<bool> did_match_high_confidence_allowlist,
      bool is_invalid_ip,
      bool is_intelligent_scan_invoked);

  // Called when token_fetcher_ has fetched the token.
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  void OnGotAccessToken(std::unique_ptr<ClientPhishingRequest> verdict,
                        std::optional<bool> did_match_high_confidence_allowlist,
                        bool is_invalid_ip,
                        const std::string& access_token);

  // Check if CSD can get an access Token. Should be enabled only for ESB
  // users, who are signed in and not in incognito mode.
  bool CanGetAccessToken();

  // Send the client report to CSD server.
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  void SendRequest(std::unique_ptr<ClientPhishingRequest> verdict,
                   const std::string& access_token,
                   std::optional<bool> did_match_high_confidence_allowlist,
                   bool is_invalid_ip);

  // Callback that is called when the server ping back is
  // done. Display an interstitial if `is_phishing` is true.
  // Otherwise, we do nothing. Called in UI thread. `is_from_cache` indicates
  // whether the warning is being shown due to a cached verdict or from an
  // actual server ping. `response_code` is cached so it can be included as
  // debugging metadata in PhishGuard pings.
  virtual void MaybeShowPhishingWarning(
      bool is_from_cache,
      ClientSideDetectionType request_type,
      std::optional<bool> did_match_high_confidence_allowlist,
      GURL phishing_url,
      bool is_phishing,
      std::optional<net::HttpStatusCode> response_code,
      std::optional<IntelligentScanVerdict> intelligent_scan_verdict);

  // Displays the platform-specific blocking page or interstitial.
  virtual void ShowBlockingPage(
      GURL phishing_url,
      ClientSideDetectionType request_type,
      std::optional<IntelligentScanVerdict> intelligent_scan_verdict,
      bool should_show_scam_warning) = 0;

  // Subclasses override this to update local feature caches with network
  // status.
  virtual void UpdateDebuggingMetadataWithNetworkResult(
      GURL phishing_url,
      net::HttpStatusCode response_code) {}

  virtual void AddReferrerChain(ClientPhishingRequest* verdict) = 0;

  // Fills in the screenshot data for the given `request`. Only fill if the
  // report type is USER_REPORT.
  virtual void MaybeFillScreenshotData(ClientPhishingRequest* verdict) {}

  // Add miscellaneous metadata to ClientPhishingRequest prior to sending the
  // ping.
  // TODO: Remove the parameter is_invalid_ip once the feature flag,
  // kClientSideDetectionLocalResourceCheckFix, is removed.
  virtual void AddMiscellaneousMetadataToClientPhishingRequest(
      ClientPhishingRequest* verdict,
      bool is_invalid_ip);

  // Records the pre-classification check result to histograms, both with and
  // without a request-type specific suffix.
  static void RecordPreClassificationCheckResultWithAndWithoutSuffix(
      PreClassificationCheckResult result,
      ClientSideDetectionType request_type);

  // Logs the ClientSideDetectionEvent event.
  void LogClientSideDetectionEvent(ClientSideDetectionEvent event,
                                   ClientSideDetectionType request_type);

  // Extracts suspicious tokens from a copied clipboard payload into a
  // structured object.
  //
  // See https://crbug.com/454952204 for the security review around clipboard
  // data extraction. UTF16 to UTF8 conversion is already done in the renderer,
  // and the payload parsing does not involve complex grammar.
  ClipboardExtractedData ExtractClipboardData(const std::u16string& payload);

  // Called when text is copied to the clipboard. Checks if the copied text
  // meets the criteria for CSD analysis and potentially starts
  // pre-classification.
  void OnTextCopiedToClipboard(const std::u16string& copied_text);

  // Whether request is forced for `current_url_`. This function also checks
  // whether enhanced protection is enabled.
  bool HasForceRequestFromRtUrlLookup();

  // Iterate through redirect chain of the current URL to see if any of the
  // sites in the chain has a llama forced request.
  void CheckRedirectChainForLlamaForcedTriggerInfo(
      ClientPhishingRequest* verdict);

  // Returns true if for a `client_side_detection_type`, the last URL is the
  // same as the last committed URL on the RenderFrameHost.
  bool HasDonePreclassificationCheckOnSameURL(
      ClientSideDetectionType client_side_detection_type);

  // Returns true if the new request type has a higher or same priority tier
  // than the last request type.
  bool NewRequestTypeTierHigher(ClientSideDetectionType new_request_type);

  // Returns the tier value for the given request type.
  int GetTierValue(ClientSideDetectionType request_type);

  // Returns whether a sample ping should be sent for allowlist-matching URLs.
  bool CanSendSamplePing(ClientSideDetectionType request_type) const;

  // Returns whether the High-Confidence allowlist match should be accepted.
  bool ShouldAcceptHCAllowlist(ClientSideDetectionType request_type,
                               bool url_on_high_confidence_allowlist) const;

  const GURL& current_url() const { return current_url_; }
  void set_current_url(const GURL& url) { current_url_ = url; }

  bool should_send_as_force_request() const {
    return should_send_as_force_request_;
  }
  void set_should_send_as_force_request(bool value) {
    should_send_as_force_request_ = value;
  }

  bool trigger_model_request_sent_as_force_request() const {
    return trigger_model_request_sent_as_force_request_;
  }
  void set_trigger_model_request_sent_as_force_request(bool value) {
    trigger_model_request_sent_as_force_request_ = value;
  }

  void set_last_committed_url(ClientSideDetectionType type, const GURL& url) {
    last_committed_url_map_[type] = url;
  }

  void clear_clipboard_extracted_data() { clipboard_extracted_data_.reset(); }

  bool is_csd_running() const { return is_csd_running_; }
  void set_is_csd_running(bool value) { is_csd_running_ = value; }

  bool is_classifying() const { return is_classifying_; }
  void set_is_classifying(bool value) { is_classifying_ = value; }

  ClientSideDetectionType last_request_type() const {
    return last_request_type_;
  }
  void set_last_request_type(ClientSideDetectionType type) {
    last_request_type_ = type;
  }

  base::TimeTicks image_embedding_start_time() const {
    return image_embedding_start_time_;
  }
  void set_image_embedding_start_time(base::TimeTicks time) {
    image_embedding_start_time_ = time;
  }

  const base::TickClock* tick_clock() const { return tick_clock_; }

 private:
  friend class ClientSideDetectionTabHelperTest;

  int GetTriggerModelVersion() const;
  bool IsModelAvailable() const;

  // MaybeTriggerCreditCardFormPing is a helper that determines whether
  // a credit card form event should trigger a CSD ping. It takes
  // an optional field_id to specify whether it is an interaction
  // or a detection trigger.
  void MaybeTriggerCreditCardFormPing(
      autofill::AutofillManager& manager,
      autofill::FormGlobalId form_id,
      std::optional<autofill::FieldGlobalId> field_id,
      std::string event_name,
      bool should_trigger);

  // This pointer may be nullptr if client-side phishing detection is
  // disabled.
  base::WeakPtr<ClientSideDetectionServiceBase> csd_service_;

  // Unowned object used for getting and caching verdicts.
  raw_ptr<VerdictCacheManager> cache_manager_ = nullptr;

  // A keyed service with profile lifetime.
  raw_ptr<IntelligentScanDelegate> intelligent_scan_delegate_ = nullptr;

  // Unowned object used for getting preference settings.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // The token fetcher used for getting access token.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  // Unowned object used for getting site history.
  raw_ptr<history::HistoryService> history_service_ = nullptr;
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observer_{this};

  // A boolean indicates whether the associated profile associated is an
  // incognito profile.
  bool is_off_the_record_ = false;

  // The current URL.
  GURL current_url_;

  // Cached result of calling HistoryService.GetDailyVisitsToOrigin
  // for some URL, used to avoid duplicate queries.
  std::optional<GURL> last_history_url_;
  std::optional<history::DailyVisitsResult> last_history_result_;

  // A boolean that indicates whether all TRIGGER_MODELS request should be
  // converted to FORCE_REQUEST. This is set true whenever the verdict cache
  // manager is checked to see if we should send as a FORCE_REQUEST.
  bool should_send_as_force_request_ = false;

  // A boolean indicates whether TRIGGER_MODELS request is sent via
  // FORCE_REQUEST. This is used to decide whether async check is allowed to
  // trigger FORCE_REQUEST.
  bool trigger_model_request_sent_as_force_request_ = false;

  // This map is used to track the last committed URL per
  // ClientSideDetectionType. This is because for some ClientSideDetectionType,
  // it can be triggered at a frequent basis per same URL.
  base::flat_map<ClientSideDetectionType, GURL> last_committed_url_map_;

  // The intelligent scan ID for the current intelligent scan request.
  std::optional<base::UnguessableToken> intelligent_scan_id_;

  // The last text that was copied to the clipboard.
  std::u16string last_copied_text_;
  std::unique_ptr<ClipboardExtractedData> clipboard_extracted_data_;

  // Modified through tests only. Initial value is set to the const
  // kProbabilityForAcceptingHCAllowlistTrigger.
  float probability_for_accepting_hc_allowlist_trigger_ =
      kProbabilityForAcceptingHCAllowlistTrigger;

  // Modified through tests only. Initial value is set to the const
  // kProbabilityForSendingSampleRequest.
  float probability_for_sending_sample_request_ =
      kProbabilityForSendingSampleRequest;

  // Track the states of the processes running and the currently running
  // ClientSideDetectionType. This begins at the CLASSIFY bucket in
  // PreClassificationCheck until just prior to the network request being sent.
  bool is_csd_running_ = false;
  bool is_classifying_ = false;
  ClientSideDetectionType last_request_type_ =
      ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED;

  // Records the start time of when image embedding started.
  base::TimeTicks image_embedding_start_time_;
  raw_ptr<const base::TickClock> tick_clock_ = nullptr;

  base::CancelableTaskTracker task_tracker_;

  // Ensures that all methods are called on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ClientSideDetectionHostBase> base_weak_factory_{this};
};
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_HOST_BASE_H_
