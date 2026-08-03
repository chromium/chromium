// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_HOST_BASE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_HOST_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/safe_browsing/core/browser/credit_card_form_event.h"
#include "components/safe_browsing/core/browser/intelligent_scan_delegate.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

class PrefService;

namespace history {
class HistoryService;
}

namespace safe_browsing {

using HostInnerTextCallback = base::OnceCallback<void(std::string)>;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ClientSideDetectionEvent {
  kTriggerStartsPreClassification = 0,
  kPreClassificationCheckComplete = 1,
  kImageClassificationBegin = 2,
  kImageClassificationComplete = 3,
  kVerdictProtoParseComplete = 4,
  kLocalModelResultComplete = 5,
  kImageEmbeddingBegin = 6,
  kImageEmbeddingComplete = 7,
  kIntelligentScanBegin = 8,
  kIntelligentScanComplete = 9,
  kMiscellaneousFieldsAdded = 10,
  kNetworkRequestSent = 11,
  kNetworkResponseReceived = 12,
  kWarningShown = 13,
  kMaxValue = kWarningShown,
};

class ClientSideDetectionFeatureCacheBase;
class ClientSideDetectionServiceBase;
class VerdictCacheManager;

class ClientSideDetectionHostBase : public autofill::AutofillManager::Observer,
                                    public history::HistoryServiceObserver {
 public:
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

  // Returns the inner text from the tab. The callback is used to retrieve a
  // string back when the inner text function is completed. This string is then
  // used to provide the intelligent scan delegate the information about the
  // page.
  virtual void GetInnerText(HostInnerTextCallback callback) = 0;

  // Calls the CSD service to classify phishing through thresholds presented in
  // `verdict`.
  virtual void ClassifyPhishingThroughThresholds(
      ClientPhishingRequest* verdict) = 0;

  // Called by `MaybeSendClientPhishingRequest` to determine whether to perform
  // image embedding as part of the client-side phishing detection flow.
  //
  // Because image embedding relies on Blink/content-specific APIs (such as
  // RenderFrameHost and Mojo interfaces), the implementation is delegated to
  // the derived class.
  //
  // If the derived class decides to perform image embedding, it will start the
  // asynchronous process and eventually send the report. If not, it must
  // forward the request to the next step in the pipeline (Intelligent Scan).
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

  void set_is_off_the_record_for_testing(bool is_off_the_record) {
    is_off_the_record_ = is_off_the_record;
  }

 protected:
  base::WeakPtr<ClientSideDetectionServiceBase> GetClientSideDetectionService()
      const;
  IntelligentScanDelegate* GetIntelligentScanDelegate() const;
  PrefService* GetPrefs() const;

  std::optional<base::UnguessableToken> GetIntelligentScanId() const;
  bool IsEnhancedProtectionEnabled() const;

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
      std::optional<IntelligentScanVerdict> intelligent_scan_verdict) = 0;

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

 private:
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

  // Track the states of the processes running and the currently running
  // ClientSideDetectionType. This begins at the CLASSIFY bucket in
  // PreClassificationCheck until just prior to the network request being sent.
  bool is_csd_running_ = false;
  bool is_classifying_ = false;
  ClientSideDetectionType last_request_type_ =
      ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED;

  base::CancelableTaskTracker task_tracker_;

  // Ensures that all methods are called on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ClientSideDetectionHostBase> base_weak_factory_{this};
};
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_HOST_BASE_H_
