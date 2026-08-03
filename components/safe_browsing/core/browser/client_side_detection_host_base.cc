// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/client_side_detection_host_base.h"

#include <string_view>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/core/browser/client_side_detection_feature_cache_base.h"
#include "components/safe_browsing/core/browser/client_side_detection_service_base.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace safe_browsing {

namespace {

// Normalizes a potential command to account for capitalization, pathing, and
// file extensions.
std::string NormalizeToken(std::u16string_view token) {
  base::FilePath path(base::FilePath::FromUTF16Unsafe(token));
  std::string filename = path.BaseName().RemoveExtension().AsUTF8Unsafe();
  return base::ToLowerASCII(filename);
}

bool IsPossibleURL(std::string token) {
  GURL url = url_formatter::FixupURL(token, "");
  if (!url.is_valid()) {
    return false;
  }

  bool has_real_domain =
      !net::registry_controlled_domains::GetDomainAndRegistry(
           url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)
           .empty();
  bool is_ip = url.HostIsIPAddress();

  return has_real_domain || is_ip;
}

// Threshold value used to skip the intelligent scan.
const int kInnerTextMinThresholdBytes = 5;

std::string_view GetRequestTypeName(
    ClientSideDetectionType client_side_detection_type) {
  switch (client_side_detection_type) {
    case safe_browsing::ClientSideDetectionType::
        CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED:
      return "Unknown";
    case safe_browsing::ClientSideDetectionType::FORCE_REQUEST:
      return "ForceRequest";
    case safe_browsing::ClientSideDetectionType::NOTIFICATION_PERMISSION_PROMPT:
      return "NotificationPermissionPrompt";
    case safe_browsing::ClientSideDetectionType::TRIGGER_MODELS:
      return "TriggerModel";
    case safe_browsing::ClientSideDetectionType::KEYBOARD_LOCK_REQUESTED:
      return "KeyboardLockRequested";
    case safe_browsing::ClientSideDetectionType::POINTER_LOCK_REQUESTED:
      return "PointerLockRequested";
    case safe_browsing::ClientSideDetectionType::VIBRATION_API:
      return "VibrationApi";
    case safe_browsing::ClientSideDetectionType::FULLSCREEN_API:
      return "FullscreenApi";
    case safe_browsing::ClientSideDetectionType::CLIPBOARD_COPY_API:
      return "ClipboardCopyApi";
    case safe_browsing::ClientSideDetectionType::CREDIT_CARD_FORM:
      return "CreditCardForm";
    case safe_browsing::ClientSideDetectionType::IMAGE_EMBEDDING_MATCH:
      return "ImageEmbeddingMatch";
    case safe_browsing::ClientSideDetectionType::USER_REPORT:
      return "UserReport";
    case safe_browsing::ClientSideDetectionType::UNFAMILIAR_LOGIN_PAGE:
      return "UnfamiliarLoginPage";
  }
}

void LogPhishingDetectionResult(ClientSideDetectionType request_type,
                                PhishingDetectorResult result,
                                std::optional<base::TimeDelta> duration) {
  std::string_view request_type_name = GetRequestTypeName(request_type);
  base::UmaHistogramEnumeration(
      "SBClientPhishing.PhishingDetectorResult", result,
      static_cast<PhishingDetectorResult>(PhishingDetectorResult_MAX + 1));
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"SBClientPhishing.PhishingDetectorResult.", request_type_name}),
      result,
      static_cast<PhishingDetectorResult>(PhishingDetectorResult_MAX + 1));
  if (duration) {
    base::UmaHistogramMediumTimes("SBClientPhishing.PhishingDetectionDuration",
                                  duration.value());
    base::UmaHistogramMediumTimes(
        base::StrCat(
            {"SBClientPhishing.PhishingDetectionDuration.", request_type_name}),
        duration.value());
  }
}

void LogLlamaForcedTriggerInfoFields(
    LlamaForcedTriggerInfo llama_forced_trigger_info) {
  base::UmaHistogramBoolean(
      "SBClientPhishing.LlamaForcedTriggerInfo.IntelligentScan",
      llama_forced_trigger_info.intelligent_scan());
  size_t rule_infos_size =
      llama_forced_trigger_info.llama_trigger_rule_infos().size();
  base::UmaHistogramCounts100(
      "SBClientPhishing.LlamaForcedTriggerInfo.LlamaTriggerRuleInfosSize",
      rule_infos_size);
  for (size_t i = 0; i < rule_infos_size; i++) {
    base::UmaHistogramCounts1000(
        "SBClientPhishing.LlamaForcedTriggerInfo.LlamaTriggerRuleId",
        llama_forced_trigger_info.llama_trigger_rule_infos()
            .at(i)
            .llama_trigger_rule_id());
  }
}

void WriteFeaturesToDisk(const ClientPhishingRequest& features,
                         const base::FilePath& base_path) {
  base::FilePath path =
      base_path.AppendASCII(base::Uuid::GenerateRandomV4().AsLowercaseString());
  base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    return;
  }
  file.WriteAtCurrentPos(base::as_byte_span(features.SerializeAsString()));
}

bool HasDebugFeatureDirectory() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kCsdDebugFeatureDirectoryFlag);
}

base::FilePath GetDebugFeatureDirectory() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kCsdDebugFeatureDirectoryFlag);
}

}  // namespace

ClientSideDetectionHostBase::ClientSideDetectionHostBase(
    base::WeakPtr<ClientSideDetectionServiceBase> csd_service,
    VerdictCacheManager* cache_manager,
    IntelligentScanDelegate* intelligent_scan_delegate,
    PrefService* pref_service,
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
    history::HistoryService* history_service,
    bool is_off_the_record)
    : csd_service_(csd_service),
      cache_manager_(cache_manager),
      intelligent_scan_delegate_(intelligent_scan_delegate),
      pref_service_(pref_service),
      token_fetcher_(std::move(token_fetcher)),
      history_service_(history_service),
      is_off_the_record_(is_off_the_record) {
  if (history_service_) {
    history_service_observer_.Observe(history_service_);
  }
}

PrefService* ClientSideDetectionHostBase::GetPrefs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_;
}

base::WeakPtr<ClientSideDetectionServiceBase>
ClientSideDetectionHostBase::GetClientSideDetectionService() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return csd_service_;
}

IntelligentScanDelegate*
ClientSideDetectionHostBase::GetIntelligentScanDelegate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return intelligent_scan_delegate_;
}

std::optional<base::UnguessableToken>
ClientSideDetectionHostBase::GetIntelligentScanId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return intelligent_scan_id_;
}

bool ClientSideDetectionHostBase::IsEnhancedProtectionEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_ &&
         safe_browsing::IsEnhancedProtectionEnabled(*pref_service_);
}

bool ClientSideDetectionHostBase::IsModelAvailable() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return csd_service_ && csd_service_->IsModelAvailable();
}

int ClientSideDetectionHostBase::GetTriggerModelVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return csd_service_ ? csd_service_->GetTriggerModelVersion() : 0;
}

void ClientSideDetectionHostBase::
    SetAndObserveHistoryServiceForTesting(  // IN-TEST
        history::HistoryService* service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  history_service_ = service;
  history_service_observer_.Reset();
  if (history_service_) {
    history_service_observer_.Observe(history_service_);
  }
}

void ClientSideDetectionHostBase::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  history_service_ = nullptr;
  history_service_observer_.Reset();
}

ClientSideDetectionHostBase::~ClientSideDetectionHostBase() = default;

void ClientSideDetectionHostBase::CancelPendingRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_tracker_.TryCancelAll();
  base_weak_factory_.InvalidateWeakPtrs();
  if (intelligent_scan_id_.has_value()) {
    intelligent_scan_delegate_->CancelIntelligentScan(*intelligent_scan_id_);
  }
}

void ClientSideDetectionHostBase::LogClientSideDetectionEvent(
    ClientSideDetectionEvent event,
    ClientSideDetectionType request_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramEnumeration("SBClientPhishing.ClientSideDetectionEvent",
                                event);
  base::UmaHistogramEnumeration(
      base::StrCat({"SBClientPhishing.ClientSideDetectionEvent.",
                    GetRequestTypeName(request_type)}),
      event);
}

void ClientSideDetectionHostBase::
    RecordPreClassificationCheckResultWithAndWithoutSuffix(
        PreClassificationCheckResult result,
        ClientSideDetectionType request_type) {
  base::UmaHistogramEnumeration("SBClientPhishing.PreClassificationCheckResult",
                                result,
                                PreClassificationCheckResult::NO_CLASSIFY_MAX);
  base::UmaHistogramEnumeration(
      base::StrCat({"SBClientPhishing.PreClassificationCheckResult.",
                    GetRequestTypeName(request_type)}),
      result, PreClassificationCheckResult::NO_CLASSIFY_MAX);
}

void ClientSideDetectionHostBase::PhishingDetectionDone(
    ClientSideDetectionType request_type,
    bool is_sample_ping,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip,
    base::TimeTicks start_time,
    PhishingDetectorResult result,
    std::optional<ClientPhishingRequest> verdict) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != PhishingDetectorResult::CLASSIFICATION_SKIPPED) {
    LogClientSideDetectionEvent(
        ClientSideDetectionEvent::kImageClassificationComplete, request_type);
    is_classifying_ = false;
  }

  ClientSideDetectionFeatureCacheBase* feature_cache_map = GetFeatureCache();

  base::TimeDelta duration = base::TimeTicks::Now() - start_time;
  LogPhishingDetectionResult(request_type, result, duration);

  if (feature_cache_map && IsEnhancedProtectionEnabled()) {
    feature_cache_map->GetOrCreateDebuggingMetadataForURL(current_url_)
        ->set_phishing_detector_result(result);
  }

  if (result == PhishingDetectorResult::CLASSIFIER_NOT_READY) {
    bool is_model_available = IsModelAvailable();
    base::UmaHistogramBoolean(
        "SBClientPhishing.BrowserReadyOnClassifierNotReady",
        is_model_available);
  } else if (feature_cache_map && IsEnhancedProtectionEnabled()) {
    // We should only add this if the classifier is ready, because then we have
    // the trigger model version in the model class.
    feature_cache_map->GetOrCreateDebuggingMetadataForURL(current_url_)
        ->set_csd_model_version(GetTriggerModelVersion());
  }

  // Send a verdict if the result is SUCCESS, or if it is CLASSIFICATION_SKIPPED
  // to indicate that phishing detection was skipped but a verdict should still
  // be sent.
  if (result != PhishingDetectorResult::CLASSIFICATION_SUCCESS &&
      result != PhishingDetectorResult::CLASSIFICATION_SKIPPED) {
    is_csd_running_ = false;
    if (request_type == ClientSideDetectionType::USER_REPORT) {
      MaybeRunUserReportCallback();
    }
    return;
  }

  base::UmaHistogramBoolean("SBClientPhishing.VerdictParseSuccessful",
                            verdict.has_value());

  if (GetClientSideDetectionService() && verdict.has_value()) {
    verdict->set_client_side_detection_type(request_type);
    LogClientSideDetectionEvent(
        ClientSideDetectionEvent::kVerdictProtoParseComplete, request_type);
    if (is_sample_ping) {
      verdict->set_report_type(ClientPhishingRequest::SAMPLE_REPORT);
    } else {
      verdict->set_report_type(ClientPhishingRequest::FULL_REPORT);
    }
    // We should only cache the verdict string if the result is SUCCESS, so that
    // in a situation where it is not, PG can retry the classification
    // because classifier can be ready or a new model is ready to address
    // the failure reasons.
    if (feature_cache_map) {
      // Initial implementation of the feature is that only PG will use the
      // cache to reuse the images that are computed by CSD-Phishing/PG. In
      // scenarios where the user reloads the page, we could use the images
      // again, and we will log to see the efficiency if we were to.
      bool cache_csd_phishing_data_available =
          feature_cache_map->GetVerdictForURL(current_url_) != nullptr;

      base::UmaHistogramBoolean(
          "SBClientPhishing.CSDPhishingCachedDataAvailable",
          cache_csd_phishing_data_available);

      feature_cache_map->InsertVerdict(
          current_url_, std::make_unique<ClientPhishingRequest>(*verdict));
    }

    MaybeSendClientPhishingRequest(
        std::make_unique<ClientPhishingRequest>(*verdict),
        did_match_high_confidence_allowlist, is_invalid_ip, result);
  } else {
    is_csd_running_ = false;
    if (request_type == ClientSideDetectionType::USER_REPORT) {
      MaybeRunUserReportCallback();
    }
  }
}

// To keep the flow consistent, we want to append additional information to the
// ClientPhishingRequest message based on feature availability in the following
// order: image embedding, intelligent scan, then token fetch. If one
// feature is not available, we will move on to the next in the order until we
// ultimately send the request.
void ClientSideDetectionHostBase::MaybeSendClientPhishingRequest(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip,
    PhishingDetectorResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string_view request_type_name =
      GetRequestTypeName(verdict->client_side_detection_type());
  if (result != PhishingDetectorResult::CLASSIFICATION_SKIPPED) {
    ClassifyPhishingThroughThresholds(verdict.get());
    base::UmaHistogramBoolean("SBClientPhishing.LocalModelDetectsPhishing",
                              verdict->is_phishing());
    base::UmaHistogramBoolean(
        base::StrCat(
            {"SBClientPhishing.LocalModelDetectsPhishing.", request_type_name}),
        verdict->is_phishing());
    LogClientSideDetectionEvent(
        ClientSideDetectionEvent::kLocalModelResultComplete,
        verdict->client_side_detection_type());
  }
  // When there is a tflite match, the target image embeddings are not
  // evaluated making the detection type effectively TRIGGER_MODELS.
  // Separately, when there is no phishing detected, the client side detection
  // type is set to TRIGGER_MODELS to simplify downstream processing.
  if (verdict->client_side_detection_type() ==
          ClientSideDetectionType::IMAGE_EMBEDDING_MATCH &&
      (verdict->is_tflite_match() || !verdict->is_phishing())) {
    verdict->set_client_side_detection_type(
        ClientSideDetectionType::TRIGGER_MODELS);
  }

  bool force_request_from_rt_url_lookup =
      verdict->client_side_detection_type() ==
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST;

  if (verdict->client_side_detection_type() ==
          ClientSideDetectionType::TRIGGER_MODELS &&
      (should_send_as_force_request_ || HasForceRequestFromRtUrlLookup())) {
    verdict->set_client_side_detection_type(
        safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
    base::UmaHistogramBoolean(
        "SBClientPhishing.TriggerModelsConvertedToForceRequestAtRequest", true);
    force_request_from_rt_url_lookup = true;
  }

  base::UmaHistogramBoolean("SBClientPhishing.RTLookupForceRequest",
                            force_request_from_rt_url_lookup);
  base::UmaHistogramExactLinear(
      "SBClientPhishing.ClientSideDetectionTypeRequest",
      verdict->client_side_detection_type(), ClientSideDetectionType_MAX + 1);

  // We add the debugging metadata relevant for PhishGuard before returning
  // whether we should proceed with sending the ping for CSD.
  if (IsEnhancedProtectionEnabled()) {
    ClientSideDetectionFeatureCacheBase* feature_cache_map = GetFeatureCache();
    LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
        feature_cache_map->GetOrCreateDebuggingMetadataForURL(current_url_);
    debugging_metadata->set_local_model_detects_phishing(
        verdict->is_phishing());
    debugging_metadata->set_forced_request(force_request_from_rt_url_lookup);
  }

  trigger_model_request_sent_as_force_request_ =
      force_request_from_rt_url_lookup;

  // We only send a phishing verdict if the verdict is phishing, the client
  // side detection type is |TRIGGER_MODELS|, AND the request is not a sample
  // ping. The detection type can be changed to FORCE_REQUEST from a
  // RTLookupResponse for a SBER/ESB user. This can also be changed when the
  // request is made from a notification permission prompt, keyboard & pointer
  // lock API.
  bool trigger_models_request_skipped =
      !verdict->is_phishing() &&
      verdict->client_side_detection_type() ==
          ClientSideDetectionType::TRIGGER_MODELS &&
      verdict->report_type() == ClientPhishingRequest::FULL_REPORT;
  if (trigger_models_request_skipped) {
    is_csd_running_ = false;
    return;
  }

  MaybeStartImageEmbedding(std::move(verdict),
                           did_match_high_confidence_allowlist, is_invalid_ip,
                           result);
}

bool ClientSideDetectionHostBase::CanGetAccessToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_off_the_record_) {
    return false;
  }

  // Return true if the primary user account of an ESB user is signed in.
  return IsEnhancedProtectionEnabled() && IsAccountSignedIn();
}

bool ClientSideDetectionHostBase::HasForceRequestFromRtUrlLookup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It is possible for the async check force request to complete before page
  // load, and we should have the URL set in case it can match in the verdict
  // cache manager.
  current_url_ = GetCurrentUrl();

  if (!cache_manager_ || !current_url_.is_valid() ||
      !IsEnhancedProtectionEnabled()) {
    return false;
  }

  if (cache_manager_->GetCachedRealTimeUrlClientSideDetectionType(
          current_url_) ==
      safe_browsing::ClientSideDetectionType::FORCE_REQUEST) {
    return true;
  }

  if (base::FeatureList::IsEnabled(
          kClientSideDetectionRedirectChainKillswitch)) {
    return false;
  }

  std::vector<GURL> redirect_chain = GetRedirectChain();

  // We pop the last element because if the redirect chain is not empty, the
  // last element will be the current URL.
  if (!redirect_chain.empty()) {
    redirect_chain.pop_back();
  }

  bool redirect_chain_contains_force_request = false;
  for (GURL url : redirect_chain) {
    if (cache_manager_->GetCachedRealTimeUrlClientSideDetectionType(url) ==
        safe_browsing::ClientSideDetectionType::FORCE_REQUEST) {
      redirect_chain_contains_force_request = true;
      break;
    }
  }

  if (!redirect_chain.empty()) {
    base::UmaHistogramBoolean(
        "SBClientPhishing.RedirectChainContainsForceRequest",
        redirect_chain_contains_force_request);
  }

  return redirect_chain_contains_force_request;
}

void ClientSideDetectionHostBase::CheckRedirectChainForLlamaForcedTriggerInfo(
    ClientPhishingRequest* verdict) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (cache_manager_ && current_url_.is_valid()) {
    safe_browsing::LlamaForcedTriggerInfo llama_forced_trigger_info;
    if (cache_manager_->GetCachedRealTimeLlamaForcedTriggerInfo(
            current_url_, &llama_forced_trigger_info)) {
      verdict->mutable_llama_forced_trigger_info()->Swap(
          &llama_forced_trigger_info);
    } else if (!base::FeatureList::IsEnabled(
                   kClientSideDetectionForcedLlamaRedirectChainKillswitch)) {
      std::vector<GURL> redirect_chain = GetRedirectChain();

      // We pop the last element because if the redirect chain is not empty,
      // the last element will be the current URL.
      if (!redirect_chain.empty()) {
        redirect_chain.pop_back();
      }

      bool redirect_chain_contains_forced_trigger_info = false;
      for (GURL url : redirect_chain) {
        if (cache_manager_->GetCachedRealTimeLlamaForcedTriggerInfo(
                url, &llama_forced_trigger_info)) {
          redirect_chain_contains_forced_trigger_info = true;
          verdict->mutable_llama_forced_trigger_info()->Swap(
              &llama_forced_trigger_info);
          break;
        }
      }

      if (!redirect_chain.empty()) {
        base::UmaHistogramBoolean(
            "SBClientPhishing.RedirectChainContainsForcedTriggerInfo",
            redirect_chain_contains_forced_trigger_info);
      }
    }
  }
}

bool ClientSideDetectionHostBase::HasDonePreclassificationCheckOnSameURL(
    ClientSideDetectionType client_side_detection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto last_committed_url =
      last_committed_url_map_.find(client_side_detection_type);
  bool has_done_url = last_committed_url != last_committed_url_map_.end() &&
                      GetCurrentUrl() == last_committed_url->second;
  return has_done_url;
}

void ClientSideDetectionHostBase::OnTextCopiedToClipboard(
    const std::u16string& copied_text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsEnhancedProtectionEnabled()) {
    return;
  }
  if (HasDonePreclassificationCheckOnSameURL(
          ClientSideDetectionType::CLIPBOARD_COPY_API)) {
    return;
  }

  base::UmaHistogramCounts10000(
      "SBClientPhishing.ClipboardCopyApi.PayloadLength", copied_text.length());

  if (copied_text.length() <
      static_cast<size_t>(kCsdClipboardCopyApiMinLength.Get())) {
    return;
  }
  if (copied_text.length() >
      static_cast<size_t>(kCsdClipboardCopyApiMaxLength.Get())) {
    return;
  }

  if (kCSDClipboardCopyApiSuspiciousTokenFilter.Get()) {
    ClipboardExtractedData extracted_data = ExtractClipboardData(copied_text);
    if (extracted_data.suspicious_tokens().empty()) {
      return;
    }
    clipboard_extracted_data_ =
        std::make_unique<ClipboardExtractedData>(std::move(extracted_data));
  } else {
    last_copied_text_ = copied_text;
    clipboard_extracted_data_.reset();
  }

  MaybeStartPreClassification(ClientSideDetectionType::CLIPBOARD_COPY_API);
}

ClipboardExtractedData ClientSideDetectionHostBase::ExtractClipboardData(
    const std::u16string& payload) {
  ClipboardExtractedData clipboard_data;
  base::TimeTicks start_time = base::TimeTicks::Now();

  bool has_loader = false;
  bool has_endpoint = false;
  bool has_runner = false;

  std::u16string processed_payload = payload;
  // Check for subcommand syntax before tokenizing, as they count as runners.
  if (processed_payload.find(u"$(") != std::u16string::npos ||
      processed_payload.find(u")") != std::u16string::npos ||
      processed_payload.find(u"`") != std::u16string::npos) {
    has_runner = true;
  }

  // Replace shell and scripting delimiters with space to simplify tokenization.
  std::vector<std::u16string> delimiters = {
      u"&&", u"||", u"$(", u"|", u";", u")", u"`", u"(", u"{", u"}", u"::"};
  for (const auto& delimiter : delimiters) {
    base::ReplaceSubstringsAfterOffset(&processed_payload, 0, delimiter, u" ");
  }

  std::vector<std::u16string> tokens =
      base::SplitString(processed_payload, u" \t\n\r", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  base::UmaHistogramMediumTimes(
      "SBClientPhishing.ClipboardCopyApi.PayloadExtraction.SplitStringDuration",
      base::TimeTicks::Now() - start_time);
  base::UmaHistogramCounts100(
      "SBClientPhishing.ClipboardCopyApi.PayloadExtraction.TokenCount",
      tokens.size());

  if (tokens.empty()) {
    return clipboard_data;
  }

  // Fetch the suspicious tokens that could be used to construct a malicious
  // command. The explanation and rationale behind these lists can be found
  // internally at go/sus-commands.
  const base::flat_set<std::string> loaders =
      base::SplitString(kCsdClipboardCopyApiLoaders.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  const base::flat_set<std::string> runners =
      base::SplitString(kCsdClipboardCopyApiRunners.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  const base::flat_set<std::string> remote_runners =
      base::SplitString(kCsdClipboardCopyApiRemoteRunners.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  const base::flat_set<std::string> decoders =
      base::SplitString(kCsdClipboardCopyApiDecoders.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (size_t i = 0; i < tokens.size(); ++i) {
    const std::string& normalized_token = NormalizeToken(tokens[i]);
    bool is_suspicious = false;

    if (decoders.contains(normalized_token)) {
      has_loader = true;
      has_endpoint = true;
      is_suspicious = true;
    }
    if (remote_runners.contains(normalized_token)) {
      has_loader = true;
      has_runner = true;
      is_suspicious = true;
    }
    if (loaders.contains(normalized_token)) {
      has_loader = true;
      is_suspicious = true;
    }
    if (runners.contains(normalized_token)) {
      has_runner = true;
      is_suspicious = true;
    }

    if (is_suspicious) {
      clipboard_data.add_suspicious_tokens(normalized_token);
      if (i == 0) {
        clipboard_data.set_is_first_token_suspicious(true);
      }
      if (i == tokens.size() - 1) {
        clipboard_data.set_is_last_token_suspicious(true);
      }
    }

    if (IsPossibleURL(base::UTF16ToUTF8(tokens[i]))) {
      has_endpoint = true;
      clipboard_data.add_urls(normalized_token);
    }
  }

  base::UmaHistogramCounts100(
      "SBClientPhishing.ClipboardCopyApi.PayloadExtraction."
      "SuspiciousTokenCount",
      clipboard_data.suspicious_tokens_size());

  clipboard_data.set_payload_length(payload.length());
  clipboard_data.set_total_parsed_tokens(tokens.size());

  // If the payload has a token to download a script, an endpoint to download
  // from, and a token to execute commands, it's overall suspicious.
  bool is_overall_suspicious = has_loader && has_endpoint && has_runner;
  base::UmaHistogramBoolean(
      "SBClientPhishing.ClipboardCopyApi.PayloadExtraction.IsOverallSuspicious",
      is_overall_suspicious);
  if (is_overall_suspicious) {
    clipboard_data.set_is_overall_suspicious(true);
    base::UmaHistogramCounts100(
        "SBClientPhishing.ClipboardCopyApi.PayloadExtraction.UrlCount",
        clipboard_data.urls_size());
    if (kCSDClipboardCopyApiIncludeFullPayload.Get()) {
      clipboard_data.set_content(base::UTF16ToUTF8(payload));
    }
  } else {
    // Otherwise, clear out any URL reporting.
    clipboard_data.clear_urls();
  }

  base::UmaHistogramMediumTimes(
      "SBClientPhishing.ClipboardCopyApi.PayloadExtraction.ProcessingDuration",
      base::TimeTicks::Now() - start_time);
  return clipboard_data;
}

void ClientSideDetectionHostBase::OnFieldTypesDetermined(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    FieldTypeSource source,
    bool small_forms_were_parsed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeTriggerCreditCardFormPing(
      manager, form, std::nullopt, "OnFieldTypesDetermined",
      kCsdCreditCardFormEnableDetectionTrigger.Get());
}

// OnAfterFocusOnFormField is an Autofill observer callback that triggers a CSD
// ping when the user interacts with a credit card form field.
void ClientSideDetectionHostBase::OnAfterFocusOnFormField(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    autofill::FieldGlobalId field_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeTriggerCreditCardFormPing(
      manager, form_id, field_id, "OnAfterFocusOnFormField",
      kCsdCreditCardFormEnableInteractionTrigger.Get());
}

void ClientSideDetectionHostBase::MaybeTriggerCreditCardFormPing(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    std::optional<autofill::FieldGlobalId> field_id,
    std::string event_name,
    bool should_trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Early exit if ESB is not enabled.
  if (!IsEnhancedProtectionEnabled()) {
    return;
  }

  // Determine whether the form was detected as a credit card form.
  const autofill::FormStructure* form = manager.FindCachedFormById(form_id);
  if (!form) {
    return;
  }

  credit_card_form::FieldDetectionHeuristic field_heuristic =
      credit_card_form::kNoDetectionHeuristic;

  if (field_id.has_value()) {
    const autofill::AutofillField* field = form->GetFieldById(*field_id);
    if (!field) {
      return;
    }
    if (autofill::GroupTypeOfFieldType(field->server_type()) ==
        autofill::FieldTypeGroup::kCreditCard) {
      field_heuristic = credit_card_form::kAutofillServer;
    } else if (autofill::GroupTypeOfFieldType(field->heuristic_type()) ==
               autofill::FieldTypeGroup::kCreditCard) {
      field_heuristic = credit_card_form::kAutofillLocal;
    }
  } else {
    // Look for fields with credit card heuristic types. Server heuristic
    // takes precedence over local heuristic.
    for (const auto& field : form->fields()) {
      if (autofill::GroupTypeOfFieldType(field->server_type()) ==
          autofill::FieldTypeGroup::kCreditCard) {
        field_heuristic = credit_card_form::kAutofillServer;
        break;
      } else if (autofill::GroupTypeOfFieldType(field->heuristic_type()) ==
                 autofill::FieldTypeGroup::kCreditCard) {
        field_heuristic = credit_card_form::kAutofillLocal;
      }
    }
  }

  if (field_heuristic == credit_card_form::kNoDetectionHeuristic) {
    return;
  }

  // Site visit count is needed as part of determining whether to send
  // a CSD ping, so look that up via HistoryService and delegate
  // handling the result to OnCreditCardFormVisitCount.
  GURL url = GetCurrentUrl();
  std::optional<history::DailyVisitsResult> cached_history_result;
  if (url == last_history_url_) {
    cached_history_result = last_history_result_;
  }
  if (history_service_ && !cached_history_result) {
    last_history_url_ = url;
    history_service_->GetDailyVisitsToOrigin(
        url::Origin::Create(url), base::Time(),
        base::Time::Now() -
            base::Minutes(kCsdCreditCardFormUserVisitLookback.Get()),
        history::VisitQuery404sPolicy::kExclude404s,
        base::BindOnce(&ClientSideDetectionHostBase::OnCreditCardFormVisitCount,
                       base_weak_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                       field_heuristic, event_name, should_trigger),
        &task_tracker_);
  } else {
    history::DailyVisitsResult history_result = cached_history_result.value_or(
        history::DailyVisitsResult{/*success=*/false});
    OnCreditCardFormVisitCount(std::nullopt, field_heuristic, event_name,
                               should_trigger, history_result);
  }
}

void ClientSideDetectionHostBase::OnCreditCardFormVisitCount(
    std::optional<base::TimeTicks> start_time,
    credit_card_form::FieldDetectionHeuristic field_heuristic,
    std::string event_name,
    bool should_trigger,
    history::DailyVisitsResult history_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_history_result_ = history_result;
  if (start_time.has_value()) {
    base::UmaHistogramTimes(
        "SBClientPhishing.HistoryServiceDuration.GetDailyVisitsToOrigin",
        base::TimeTicks::Now() - start_time.value());
  }

  credit_card_form::SiteVisit site_visit = credit_card_form::kUnknownSiteVisit;
  if (history_result.success) {
    site_visit = history_result.total_visits >
                         static_cast<int>(kCsdCreditCardFormMaxUserVisit.Get())
                     ? credit_card_form::kRepeatSiteVisit
                     : credit_card_form::kNewSiteVisit;
  }

  credit_card_form::ReferringApp referring_app = GetReferringApp();

  credit_card_form::LogEvent(site_visit, referring_app, field_heuristic,
                             event_name);

  // Do not proceed with preclassification if it has already been done for
  // CREDIT_CARD_FORM on this URL. Only the first credit card event on this
  // page will go through preclassification and none further.
  if (HasDonePreclassificationCheckOnSameURL(
          ClientSideDetectionType::CREDIT_CARD_FORM)) {
    return;
  }

  // Log the event after URL deduplication to provide event telemetry that
  // corresponds to the preclassification check.
  credit_card_form::LogDedupedEvent(site_visit, referring_app, field_heuristic,
                                    event_name);

  // Early exit if the event should not trigger a CSD ping.
  if (!should_trigger) {
    return;
  }

  // Early exit if the user has visited this site before.
  if (kCsdCreditCardFormEnableNewSiteFilter.Get() &&
      site_visit == credit_card_form::kRepeatSiteVisit) {
    return;
  }

  // Early exit if the credit card form was detected using a server heuristic.
  if (kCsdCreditCardFormEnableHeuristicFilter.Get() &&
      field_heuristic == credit_card_form::kAutofillServer) {
    return;
  }

  if (kCsdCreditCardFormEnableReferringAppFilter.Get()) {
    // Early exit if referring app is not an SMS app. On non-Android platforms,
    // we do not have referring app info, so this will always exit early on
    // those platforms.
    if (referring_app != credit_card_form::ReferringApp::kSmsApp) {
      return;
    }
  }

  MaybeStartPreClassification(ClientSideDetectionType::CREDIT_CARD_FORM);
}

void ClientSideDetectionHostBase::
    AddMiscellaneousMetadataToClientPhishingRequest(
        ClientPhishingRequest* verdict,
        bool is_invalid_ip) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  *verdict->mutable_population() = GetUserPopulation();
  verdict->mutable_population()->add_finch_active_groups(
      base::FeatureList::IsEnabled(kConditionalImageResize)
          ? "ConditionalImageResize.Enabled"
          : "ConditionalImageResize.Control");

  if (base::FeatureList::IsEnabled(kClientSideDetectionNewObservers)) {
    verdict->mutable_population()->add_finch_active_groups(
        "ClientSideDetectionNewObservers.Enabled." +
        base::NumberToString(kCsdClassificationDelay.Get()));
  } else {
    verdict->mutable_population()->add_finch_active_groups(
        "ClientSideDetectionNewObservers.Control");
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionLocalResourceCheckFix)) {
    if (is_invalid_ip) {
      verdict->mutable_population()->add_finch_active_groups(
          "ClientSideDetectionLocalResourceCheckFix.Enabled");
    }
  } else {
    verdict->mutable_population()->add_finch_active_groups(
        "ClientSideDetectionLocalResourceCheckFix.Control");
  }

  if (cache_manager_) {
    ChromeUserPopulation::PageLoadToken token =
        cache_manager_->GetPageLoadToken(current_url_);
    // It's possible that the token is not found because real time URL check
    // is not performed for this navigation. Create a new page load token in
    // this case.
    if (!token.has_token_value()) {
      token = cache_manager_->CreatePageLoadToken(current_url_);
    }
    verdict->mutable_population()->mutable_page_load_tokens()->Add()->Swap(
        &token);
  }

  if (verdict->client_side_detection_type() ==
      ClientSideDetectionType::CLIPBOARD_COPY_API) {
    if (base::FeatureList::IsEnabled(kClientSideDetectionClipboardCopyApi) &&
        kCSDClipboardCopyApiProcessPayload.Get()) {
      if (clipboard_extracted_data_) {
        *verdict->mutable_clipboard_extracted_data() =
            *clipboard_extracted_data_;
      } else {
        *verdict->mutable_clipboard_extracted_data() =
            ExtractClipboardData(last_copied_text_);
      }
    }
  }

  MaybeFillScreenshotData(verdict);

  if (IsEnhancedProtectionEnabled()) {
    AddReferrerChain(verdict);
  }

  if (HasDebugFeatureDirectory()) {
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::BindOnce(&WriteFeaturesToDisk, *verdict,
                                              GetDebugFeatureDirectory()));
  }
}

void ClientSideDetectionHostBase::SendRequest(
    std::unique_ptr<ClientPhishingRequest> verdict,
    const std::string& access_token,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AddMiscellaneousMetadataToClientPhishingRequest(verdict.get(), is_invalid_ip);
  LogClientSideDetectionEvent(
      ClientSideDetectionEvent::kMiscellaneousFieldsAdded,
      verdict->client_side_detection_type());

  MaybeStartGeminiAntiscamProtection(GURL(verdict->url()),
                                     verdict->client_side_detection_type(),
                                     did_match_high_confidence_allowlist);

  is_csd_running_ = false;

  LogClientSideDetectionEvent(ClientSideDetectionEvent::kNetworkRequestSent,
                              verdict->client_side_detection_type());
  if (verdict->client_side_detection_type() ==
      ClientSideDetectionType::USER_REPORT) {
    MaybeRunUserReportCallback();
  }

  auto callback = base::BindOnce(
      &ClientSideDetectionHostBase::MaybeShowPhishingWarning,
      base_weak_factory_.GetWeakPtr(),
      /*is_from_cache=*/false, verdict->client_side_detection_type(),
      did_match_high_confidence_allowlist);

  base::WeakPtr<ClientSideDetectionServiceBase> csd_service =
      GetClientSideDetectionService();
  if (csd_service) {
    csd_service->SendClientReportPhishingRequest(
        std::move(verdict), std::move(callback), access_token);
  }
}

bool ClientSideDetectionHostBase::NewRequestTypeTierHigher(
    ClientSideDetectionType new_request_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (last_request_type_ ==
      ClientSideDetectionType::CLIENT_SIDE_DETECTION_TYPE_UNSPECIFIED) {
    return true;
  }

  if (base::FeatureList::IsEnabled(kClientSideDetectionBypassTiers)) {
    std::string bypass_tiers_list_str =
        kClientSideDetectionBypassTiersList.Get();
    if (!bypass_tiers_list_str.empty()) {
      std::vector<std::string> bypass_tiers =
          base::SplitString(bypass_tiers_list_str, ",", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_NONEMPTY);
      for (const std::string& tier_str : bypass_tiers) {
        int tier_val;
        if (base::StringToInt(tier_str, &tier_val) &&
            static_cast<int>(new_request_type) == tier_val) {
          return true;
        }
      }
    }
  }

  return GetTierValue(new_request_type) < GetTierValue(last_request_type_);
}

int ClientSideDetectionHostBase::GetTierValue(
    ClientSideDetectionType request_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetClientSideDetectionTypeTier(request_type);
}

void ClientSideDetectionHostBase::MaybeStartIntelligentScanForScamDetection(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip) {
  // Use the address of the verdict object as the unique track_id.
  TRACE_EVENT_BEGIN(
      /*category=*/"safe_browsing",
      /*name=*/"IntelligentScanScamDetection",
      perfetto::NamedTrack::FromPointer("safe_browsing::ClientPhishingRequest",
                                        verdict.get()));

  if (verdict->client_side_detection_type() ==
      ClientSideDetectionType::FORCE_REQUEST) {
    CheckRedirectChainForLlamaForcedTriggerInfo(verdict.get());
    base::UmaHistogramBoolean(
        "SBClientPhishing.RTLookupForceRequest.HasLlamaForcedTriggerInfo",
        verdict->has_llama_forced_trigger_info());
  }

  if (verdict->has_llama_forced_trigger_info()) {
    LogLlamaForcedTriggerInfoFields(verdict->llama_forced_trigger_info());
  }

  if (intelligent_scan_delegate_->ShouldRequestIntelligentScan(verdict.get())) {
    if (did_match_high_confidence_allowlist.has_value() &&
        did_match_high_confidence_allowlist.value()) {
      IntelligentScanInfo intelligent_scan_info;
      intelligent_scan_info.set_no_info_reason(
          IntelligentScanInfo::ALLOWLISTED);
      *verdict->mutable_intelligent_scan_info() =
          std::move(intelligent_scan_info);
      MaybeGetAccessToken(std::move(verdict),
                          did_match_high_confidence_allowlist, is_invalid_ip,
                          /*is_intelligent_scan_invoked=*/false);
      return;
    }

    IntelligentScanDelegate::ModelType model_type =
        intelligent_scan_delegate_->GetIntelligentScanModelType(
            /*log_failed_eligibility_reason=*/true);
    bool intelligent_scan_eligible =
        IntelligentScanDelegate::IsIntelligentScanAvailable(model_type);

    base::UmaHistogramBoolean(
        "SBClientPhishing.IsIntelligentScanAvailableAtInquiryTime",
        intelligent_scan_eligible);
    base::UmaHistogramBoolean(
        base::StrCat(
            {"SBClientPhishing.IsIntelligentScanAvailableAtInquiryTime.",
             GetRequestTypeName(verdict->client_side_detection_type())}),
        intelligent_scan_eligible);

    if (!intelligent_scan_eligible) {
      IntelligentScanInfo intelligent_scan_info;
      switch (model_type) {
        case IntelligentScanDelegate::ModelType::kNotSupportedOnDevice:
          intelligent_scan_info.set_no_info_reason(
              IntelligentScanInfo::ON_DEVICE_MODEL_UNAVAILABLE);
          break;
        case IntelligentScanDelegate::ModelType::kNotSupportedServerSide:
          intelligent_scan_info.set_no_info_reason(
              IntelligentScanInfo::SERVER_SIDE_MODEL_UNAVAILABLE);
          break;
        case IntelligentScanDelegate::ModelType::kOnDevice:
        case IntelligentScanDelegate::ModelType::kServerSide:
          NOTREACHED();
      }
      *verdict->mutable_intelligent_scan_info() =
          std::move(intelligent_scan_info);
      MaybeGetAccessToken(std::move(verdict),
                          did_match_high_confidence_allowlist, is_invalid_ip,
                          /*is_intelligent_scan_invoked=*/false);
      return;
    }

    GetInnerText(
        base::BindOnce(&ClientSideDetectionHostBase::OnInnerTextComplete,
                       base_weak_factory_.GetWeakPtr(), std::move(verdict),
                       did_match_high_confidence_allowlist, is_invalid_ip));
    return;
  }

  MaybeGetAccessToken(std::move(verdict), did_match_high_confidence_allowlist,
                      is_invalid_ip, /*is_intelligent_scan_invoked=*/false);
}

void ClientSideDetectionHostBase::OnInnerTextComplete(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip,
    std::string inner_text) {
  base::UmaHistogramCounts100000(
      "SBClientPhishing.IntelligentScanInnerTextSize", inner_text.size());
  base::UmaHistogramCounts100000(
      base::StrCat({"SBClientPhishing.IntelligentScanInnerTextSize.",
                    GetRequestTypeName(verdict->client_side_detection_type())}),
      inner_text.size());
  if (inner_text.size() <= kInnerTextMinThresholdBytes) {
    IntelligentScanInfo intelligent_scan_info;
    if (inner_text.empty()) {
      intelligent_scan_info.set_no_info_reason(IntelligentScanInfo::EMPTY_TEXT);

    } else {
      intelligent_scan_info.set_no_info_reason(
          IntelligentScanInfo::TEXT_TOO_SHORT);
    }
    *verdict->mutable_intelligent_scan_info() =
        std::move(intelligent_scan_info);
    MaybeGetAccessToken(std::move(verdict), did_match_high_confidence_allowlist,
                        is_invalid_ip, /*is_intelligent_scan_invoked=*/false);
    return;
  }

  LogClientSideDetectionEvent(ClientSideDetectionEvent::kIntelligentScanBegin,
                              verdict->client_side_detection_type());
  intelligent_scan_id_ = intelligent_scan_delegate_->StartIntelligentScan(
      inner_text,
      base::BindOnce(&ClientSideDetectionHostBase::OnIntelligentScanDone,
                     base_weak_factory_.GetWeakPtr(), std::move(verdict),
                     did_match_high_confidence_allowlist, is_invalid_ip));
}

void ClientSideDetectionHostBase::OnIntelligentScanDone(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip,
    IntelligentScanDelegate::IntelligentScanResult response) {
  LogClientSideDetectionEvent(
      ClientSideDetectionEvent::kIntelligentScanComplete,
      verdict->client_side_detection_type());
  intelligent_scan_id_.reset();

  base::UmaHistogramBoolean(
      "SBClientPhishing.IntelligentScanHasSuccessfulResponse",
      response.execution_success);
  base::UmaHistogramBoolean(
      base::StrCat({"SBClientPhishing.IntelligentScanHasSuccessfulResponse.",
                    GetRequestTypeName(verdict->client_side_detection_type())}),
      response.execution_success);

  IntelligentScanInfo intelligent_scan_info;
  if (response.execution_success) {
    intelligent_scan_info.set_brand(response.brand);
    intelligent_scan_info.set_intent(response.intent);
    if (base::FeatureList::IsEnabled(kClientSideDetectionScamScore) &&
        response.scam_score.has_value()) {
      intelligent_scan_info.set_scam_score(response.scam_score.value());
    }
  } else {
    intelligent_scan_info.set_no_info_reason(response.no_info_reason);
  }

  if (response.model_version != IntelligentScanDelegate::IntelligentScanResult::
                                    kModelVersionUnavailable) {
    intelligent_scan_info.set_model_version(response.model_version);
  }

  switch (response.model_type) {
    case IntelligentScanDelegate::ModelType::kNotSupportedOnDevice:
    case IntelligentScanDelegate::ModelType::kNotSupportedServerSide:
      intelligent_scan_info.set_model_type(
          IntelligentScanModelType::NOT_SUPPORTED);
      break;
    case IntelligentScanDelegate::ModelType::kOnDevice:
      intelligent_scan_info.set_model_type(
          IntelligentScanModelType::ON_DEVICE_MODEL);
      break;
    case IntelligentScanDelegate::ModelType::kServerSide:
      intelligent_scan_info.set_model_type(
          IntelligentScanModelType::SERVER_SIDE_MODEL);
      break;
  }

  *verdict->mutable_intelligent_scan_info() = std::move(intelligent_scan_info);

  MaybeGetAccessToken(std::move(verdict), did_match_high_confidence_allowlist,
                      is_invalid_ip, /*is_intelligent_scan_invoked=*/true);
}

void ClientSideDetectionHostBase::MaybeGetAccessToken(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip,
    bool is_intelligent_scan_invoked) {
  TRACE_EVENT_END(
      /*category=*/"safe_browsing",
      perfetto::NamedTrack::FromPointer("safe_browsing::ClientPhishingRequest",
                                        verdict.get()),
      /*arg=*/"inquired_intelligent_scan",
      /*value=*/is_intelligent_scan_invoked);
  if (CanGetAccessToken()) {
    token_fetcher_->Start(
        base::BindOnce(&ClientSideDetectionHostBase::OnGotAccessToken,
                       base_weak_factory_.GetWeakPtr(), std::move(verdict),
                       did_match_high_confidence_allowlist, is_invalid_ip));
    return;
  }

  std::string empty_access_token;
  SendRequest(std::move(verdict), empty_access_token,
              did_match_high_confidence_allowlist, is_invalid_ip);
}

void ClientSideDetectionHostBase::OnGotAccessToken(
    std::unique_ptr<ClientPhishingRequest> verdict,
    std::optional<bool> did_match_high_confidence_allowlist,
    bool is_invalid_ip,
    const std::string& access_token) {
  SendRequest(std::move(verdict), access_token,
              did_match_high_confidence_allowlist, is_invalid_ip);
}

}  // namespace safe_browsing
