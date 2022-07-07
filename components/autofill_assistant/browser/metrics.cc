// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/metrics.h"

#include <algorithm>
#include <numeric>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/intent_strings.h"
#include "components/autofill_assistant/browser/startup_util.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {
namespace {
int64_t ToEntryCountBucket(int entry_count) {
  DCHECK_GE(entry_count, 0);
  if (entry_count < 5)
    return static_cast<int64_t>(entry_count);

  return static_cast<int64_t>(Metrics::UserDataEntryCount::FIVE_OR_MORE);
}
}  // namespace

// Intent not set constant.
const char* const kIntentNotSet = "NotSet";

namespace {
const char kDropOut[] = "Android.AutofillAssistant.DropOutReason";
const char kTtsButtonAction[] =
    "Android.AutofillAssistant.TextToSpeech.ButtonAction";
const char kTtsEngineEvent[] =
    "Android.AutofillAssistant.TextToSpeech.EngineEvent";
const char kFeatureModuleInstallation[] =
    "Android.AutofillAssistant.FeatureModuleInstallation";
const char kPaymentRequestPrefilled[] =
    "Android.AutofillAssistant.PaymentRequest.Prefilled";
const char kPaymentRequestAutofillInfoChanged[] =
    "Android.AutofillAssistant.PaymentRequest.AutofillChanged";
const char kPaymentRequestFirstNameOnly[] =
    "Android.AutofillAssistant.PaymentRequest.FirstNameOnly";
const char kCupRpcVerificationEvent[] =
    "Android.AutofillAssistant.CupRpcVerificationEvent";
const char kJsFlowStartedEvent[] =
    "Android.AutofillAssistant.JsFlowStartedEvent";
const char kDependenciesInvalidated[] =
    "Android.AutofillAssistant.DependenciesInvalidated";
const char kOnboardingFetcherResultStatus[] =
    "Android.AutofillAssistant.OnboardingFetcher.ResultStatus";
const char kServiceRequestSuccessRetryCount[] =
    "Android.AutofillAssistant.ServiceRequestSender.SuccessRetryCount";
const char kServiceRequestFailureRetryCount[] =
    "Android.AutofillAssistant.ServiceRequestSender.FailureRetryCount";
static bool DROPOUT_RECORDED = false;

std::string GetSuffixForIntent(const std::string& intent) {
  base::flat_map<std::string, std::string> histogramsSuffixes = {
      {kBuyMovieTicket, ".BuyMovieTicket"},
      {kFlightsCheckin, ".FlightsCheckin"},
      {kFoodOrdering, ".FoodOrdering"},
      {kFoodOrderingDelivery, ".FoodOrderingDelivery"},
      {kFoodOrderingPickup, ".FoodOrderingPickup"},
      {kPasswordChange, ".PasswordChange"},
      {kRentCar, ".RentCar"},
      {kShopping, ".Shopping"},
      {kShoppingAssistedCheckout, ".ShoppingAssistedCheckout"},
      {kTeleport, ".Teleport"},
      {kIntentNotSet, ".NotSet"}};

  // Check if histogram exists for given intent.
  if (histogramsSuffixes.count(intent) == 0) {
    DVLOG(2) << "Unknown intent " << intent;
    return ".UnknownIntent";
  }
  return histogramsSuffixes[intent];
}

// Extracts the enum value corresponding to the caller specified in
// |script_parameters|.
Metrics::AutofillAssistantCaller ExtractCallerFromScriptParameters(
    const ScriptParameters& script_parameters) {
  auto caller = script_parameters.GetCaller();
  if (!caller ||
      *caller >
          static_cast<int64_t>(Metrics::AutofillAssistantCaller::kMaxValue) ||
      *caller < 0) {
    return Metrics::AutofillAssistantCaller::UNKNOWN_CALLER;
  }

  return static_cast<Metrics::AutofillAssistantCaller>(*caller);
}

// Extracts the enum value corresponding to the source specified in
// |script_parameters|.
Metrics::AutofillAssistantSource ExtractSourceFromScriptParameters(
    const ScriptParameters& script_parameters) {
  auto source = script_parameters.GetSource();
  if (!source ||
      *source >
          static_cast<int64_t>(Metrics::AutofillAssistantSource::kMaxValue) ||
      *source < 0) {
    return Metrics::AutofillAssistantSource::UNKNOWN_SOURCE;
  }

  return static_cast<Metrics::AutofillAssistantSource>(*source);
}

// Extracts the list of experiments specified in |script_parameters|, if any.
// Returns a bit-wise OR of the running experiments.
int64_t ExtractExperimentsFromScriptParameters(
    const ScriptParameters& script_parameters) {
  std::vector<std::string> experiments = script_parameters.GetExperiments();
  if (experiments.empty()) {
    return static_cast<int64_t>(
        Metrics::AutofillAssistantExperiment::NO_EXPERIMENT);
  }

  // This will be bit-wise OR of running experiments. Currently, there are no
  // known experiments.
  return static_cast<int64_t>(
      Metrics::AutofillAssistantExperiment::UNKNOWN_EXPERIMENT);
}

Metrics::AutofillAssistantStarted ToAutofillAssistantStarted(
    StartupMode event) {
  switch (event) {
    case StartupMode::FEATURE_DISABLED:
      return Metrics::AutofillAssistantStarted::FAILED_FEATURE_DISABLED;
    case StartupMode::MANDATORY_PARAMETERS_MISSING:
      return Metrics::AutofillAssistantStarted::
          FAILED_MANDATORY_PARAMETER_MISSING;
    case StartupMode::SETTING_DISABLED:
      return Metrics::AutofillAssistantStarted::FAILED_SETTING_DISABLED;
    case StartupMode::NO_INITIAL_URL:
      return Metrics::AutofillAssistantStarted::FAILED_NO_INITIAL_URL;
    case StartupMode::START_REGULAR:
      return Metrics::AutofillAssistantStarted::OK_IMMEDIATE_START;
    case StartupMode::START_RPC_TRIGGER_SCRIPT:
      return Metrics::AutofillAssistantStarted::OK_DELAYED_START;
  }
}

}  // namespace

// static
void Metrics::RecordDropOut(DropOutReason reason, const std::string& intent) {
  // TODO(arbesser): use an RAII token instead of a static variable to ensure
  // that dropout recording happens exactly once per startup attempt.
  DCHECK_LE(reason, DropOutReason::kMaxValue);
  if (DROPOUT_RECORDED) {
    return;
  }

  auto suffix = GetSuffixForIntent(intent.empty() ? kIntentNotSet : intent);
  base::UmaHistogramEnumeration(kDropOut + suffix, reason);
  base::UmaHistogramEnumeration(kDropOut, reason);
  if (reason != DropOutReason::AA_START) {
    DVLOG(3) << "Drop out with reason: " << reason;
    DROPOUT_RECORDED = true;
  }
}

// static
void Metrics::RecordPaymentRequestPrefilledSuccess(
    bool initially_complete,
    CollectUserDataResult result) {
  switch (result) {
    case Metrics::CollectUserDataResult::UNKNOWN:
      if (initially_complete) {
        base::UmaHistogramEnumeration(
            kPaymentRequestPrefilled,
            PaymentRequestPrefilled::PREFILLED_UNKNOWN);
        return;
      }
      base::UmaHistogramEnumeration(
          kPaymentRequestPrefilled,
          PaymentRequestPrefilled::NOTPREFILLED_UNKNOWN);
      return;
    case Metrics::CollectUserDataResult::FAILURE:
      if (initially_complete) {
        base::UmaHistogramEnumeration(
            kPaymentRequestPrefilled,
            PaymentRequestPrefilled::PREFILLED_FAILURE);
        return;
      }
      base::UmaHistogramEnumeration(
          kPaymentRequestPrefilled,
          PaymentRequestPrefilled::NOTPREFILLED_FAILURE);
      return;
    case Metrics::CollectUserDataResult::SUCCESS:
      if (initially_complete) {
        base::UmaHistogramEnumeration(
            kPaymentRequestPrefilled,
            PaymentRequestPrefilled::PREFILLED_SUCCESS);
        return;
      }
      base::UmaHistogramEnumeration(
          kPaymentRequestPrefilled,
          PaymentRequestPrefilled::NOTPREFILLED_SUCCESS);
      return;
    case Metrics::CollectUserDataResult::TERMS_AND_CONDITIONS_LINK_CLICKED:
      if (initially_complete) {
        base::UmaHistogramEnumeration(
            kPaymentRequestPrefilled,
            PaymentRequestPrefilled::
                PREFILLED_TERMS_AND_CONDITIONS_LINK_CLICKED);
        return;
      }
      base::UmaHistogramEnumeration(
          kPaymentRequestPrefilled,
          PaymentRequestPrefilled::
              NOTPREFILLED_TERMS_AND_CONDITIONS_LINK_CLICKED);
      return;
    case Metrics::CollectUserDataResult::ADDITIONAL_ACTION_SELECTED:
      if (initially_complete) {
        base::UmaHistogramEnumeration(
            kPaymentRequestPrefilled,
            PaymentRequestPrefilled::PREFILLED_ADDITIONAL_ACTION_SELECTED);
        return;
      }
      base::UmaHistogramEnumeration(
          kPaymentRequestPrefilled,
          PaymentRequestPrefilled::NOTPREFILLED_ADDITIONAL_ACTION_SELECTED);
      return;
  }
}

// static
void Metrics::RecordPaymentRequestAutofillChanged(
    bool changed,
    CollectUserDataResult result) {
  switch (result) {
    case Metrics::CollectUserDataResult::UNKNOWN:
      if (changed) {
        base::UmaHistogramEnumeration(
            kPaymentRequestAutofillInfoChanged,
            PaymentRequestAutofillInfoChanged::CHANGED_UNKNOWN);
        return;
      }
      base::UmaHistogramEnumeration(
          kPaymentRequestAutofillInfoChanged,
          PaymentRequestAutofillInfoChanged::NOTCHANGED_UNKNOWN);
      return;
    case Metrics::CollectUserDataResult::FAILURE:
      if (changed) {
        base::UmaHistogramEnumeration(
            kPaymentRequestAutofillInfoChanged,
            PaymentRequestAutofillInfoChanged::CHANGED_FAILURE);
        return;
      }
      base::UmaHistogramEnumeration(
          kPaymentRequestAutofillInfoChanged,
          PaymentRequestAutofillInfoChanged::NOTCHANGED_FAILURE);
      return;
    case Metrics::CollectUserDataResult::SUCCESS:
      if (changed) {
        base::UmaHistogramEnumeration(
            kPaymentRequestAutofillInfoChanged,
            PaymentRequestAutofillInfoChanged::CHANGED_SUCCESS);
        return;
      }
      base::UmaHistogramEnumeration(
          kPaymentRequestAutofillInfoChanged,
          PaymentRequestAutofillInfoChanged::NOTCHANGED_SUCCESS);
      return;
    case Metrics::CollectUserDataResult::TERMS_AND_CONDITIONS_LINK_CLICKED:
      if (changed) {
        base::UmaHistogramEnumeration(
            kPaymentRequestAutofillInfoChanged,
            PaymentRequestAutofillInfoChanged::
                CHANGED_TERMS_AND_CONDITIONS_LINK_CLICKED);
        return;
      }
      base::UmaHistogramEnumeration(
          kPaymentRequestAutofillInfoChanged,
          PaymentRequestAutofillInfoChanged::
              NOTCHANGED_TERMS_AND_CONDITIONS_LINK_CLICKED);
      return;
    case Metrics::CollectUserDataResult::ADDITIONAL_ACTION_SELECTED:
      if (changed) {
        base::UmaHistogramEnumeration(kPaymentRequestAutofillInfoChanged,
                                      PaymentRequestAutofillInfoChanged::
                                          CHANGED_ADDITIONAL_ACTION_SELECTED);
        return;
      }
      base::UmaHistogramEnumeration(kPaymentRequestAutofillInfoChanged,
                                    PaymentRequestAutofillInfoChanged::
                                        NOTCHANGED_ADDITIONAL_ACTION_SELECTED);
      return;
  }
}

// static
void Metrics::RecordPaymentRequestFirstNameOnly(bool first_name_only) {
  base::UmaHistogramBoolean(kPaymentRequestFirstNameOnly, first_name_only);
}

// static
void Metrics::RecordTriggerScriptStarted(ukm::UkmRecorder* ukm_recorder,
                                         ukm::SourceId source_id,
                                         TriggerScriptStarted event) {
  ukm::builders::AutofillAssistant_LiteScriptStarted(source_id)
      .SetLiteScriptStarted(static_cast<int64_t>(event))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordTriggerScriptStarted(ukm::UkmRecorder* ukm_recorder,
                                         ukm::SourceId source_id,
                                         StartupMode startup_mode,
                                         bool feature_module_installed,
                                         bool is_first_time_user) {
  TriggerScriptStarted event;
  switch (startup_mode) {
    case StartupMode::FEATURE_DISABLED:
      if (base::FeatureList::IsEnabled(
              features::kAutofillAssistantProactiveHelp) &&
          !feature_module_installed) {
        event = TriggerScriptStarted::DFM_UNAVAILABLE;
      } else {
        event = TriggerScriptStarted::FEATURE_DISABLED;
      }
      break;
    case StartupMode::SETTING_DISABLED:
      event = TriggerScriptStarted::PROACTIVE_TRIGGERING_DISABLED;
      break;
    case StartupMode::NO_INITIAL_URL:
      event = TriggerScriptStarted::NO_INITIAL_URL;
      break;
    case StartupMode::MANDATORY_PARAMETERS_MISSING:
      event = TriggerScriptStarted::MANDATORY_PARAMETER_MISSING;
      break;
    case StartupMode::START_RPC_TRIGGER_SCRIPT:
      event = is_first_time_user ? TriggerScriptStarted::FIRST_TIME_USER
                                 : TriggerScriptStarted::RETURNING_USER;
      break;
    case StartupMode::START_REGULAR:
      // Regular starts do not record impressions for |TriggerScriptStarted|.
      return;
  }

  RecordTriggerScriptStarted(ukm_recorder, source_id, event);
}

// static
void Metrics::RecordTriggerScriptFinished(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    TriggerScriptProto::TriggerUIType trigger_ui_type,
    TriggerScriptFinishedState event) {
  ukm::builders::AutofillAssistant_LiteScriptFinished(source_id)
      .SetTriggerUIType(static_cast<int64_t>(trigger_ui_type))
      .SetLiteScriptFinished(static_cast<int64_t>(event))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordTriggerScriptShownToUser(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    TriggerScriptProto::TriggerUIType trigger_ui_type,
    TriggerScriptShownToUser event) {
  ukm::builders::AutofillAssistant_LiteScriptShownToUser(source_id)
      .SetTriggerUIType(static_cast<int64_t>(trigger_ui_type))
      .SetLiteScriptShownToUser(static_cast<int64_t>(event))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordTriggerScriptOnboarding(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    TriggerScriptProto::TriggerUIType trigger_ui_type,
    TriggerScriptOnboarding event) {
  ukm::builders::AutofillAssistant_LiteScriptOnboarding(source_id)
      .SetTriggerUIType(static_cast<int64_t>(trigger_ui_type))
      .SetLiteScriptOnboarding(static_cast<int64_t>(event))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordRegularScriptOnboarding(ukm::UkmRecorder* ukm_recorder,
                                            ukm::SourceId source_id,
                                            Metrics::Onboarding event) {
  ukm::builders::AutofillAssistant_RegularScriptOnboarding(source_id)
      .SetOnboarding(static_cast<int64_t>(event))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordInChromeTriggerAction(ukm::UkmRecorder* ukm_recorder,
                                          ukm::SourceId source_id,
                                          InChromeTriggerAction event) {
  ukm::builders::AutofillAssistant_InChromeTriggering(source_id)
      .SetInChromeTriggerAction(static_cast<int64_t>(event))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordTtsButtonAction(TtsButtonAction action) {
  DCHECK_LE(action, TtsButtonAction::kMaxValue);
  base::UmaHistogramEnumeration(kTtsButtonAction, action);
}

// static
void Metrics::RecordTtsEngineEvent(TtsEngineEvent event) {
  DCHECK_LE(event, TtsEngineEvent::kMaxValue);
  base::UmaHistogramEnumeration(kTtsEngineEvent, event);
}

// static
void Metrics::RecordFeatureModuleInstallation(FeatureModuleInstallation event) {
  DCHECK_LE(event, FeatureModuleInstallation::kMaxValue);
  base::UmaHistogramEnumeration(kFeatureModuleInstallation, event);
}

// static
void Metrics::RecordTriggerConditionEvaluationTime(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    base::TimeDelta evaluation_time) {
  ukm::builders::AutofillAssistant_Timing(source_id)
      .SetTriggerConditionEvaluationMs(evaluation_time.InMilliseconds())
      .Record(ukm_recorder);
}

// static
void Metrics::RecordDependenciesInvalidated(
    DependenciesInvalidated dependencies_invalidated) {
  DCHECK_LE(dependencies_invalidated, DependenciesInvalidated::kMaxValue);
  base::UmaHistogramEnumeration(kDependenciesInvalidated,
                                dependencies_invalidated);
}

// static
void Metrics::RecordStartRequest(ukm::UkmRecorder* ukm_recorder,
                                 ukm::SourceId source_id,
                                 const ScriptParameters& script_parameters,
                                 StartupMode event) {
  ukm::builders::AutofillAssistant_StartRequest(source_id)
      .SetCaller(static_cast<int64_t>(
          ExtractCallerFromScriptParameters(script_parameters)))
      .SetSource(static_cast<int64_t>(
          ExtractSourceFromScriptParameters(script_parameters)))
      .SetIntent(static_cast<int64_t>(
          ExtractIntentFromScriptParameters(script_parameters)))
      .SetExperiments(ExtractExperimentsFromScriptParameters(script_parameters))
      .SetStarted(static_cast<int64_t>(ToAutofillAssistantStarted(event)))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordContactMetrics(ukm::UkmRecorder* ukm_recorder,
                                   ukm::SourceId source_id,
                                   int complete_count,
                                   int incomplete_count,
                                   int initially_selected_field_bitmask,
                                   UserDataSelectionState selection_state) {
  ukm::builders::AutofillAssistant_CollectContact(source_id)
      .SetCompleteContactProfilesCount(ToEntryCountBucket(complete_count))
      .SetIncompleteContactProfilesCount(ToEntryCountBucket(incomplete_count))
      .SetInitialContactFieldsStatus(initially_selected_field_bitmask)
      .SetContactModified(static_cast<int64_t>(selection_state))
      .Record(ukm_recorder);
}

void Metrics::RecordCreditCardMetrics(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    int complete_count,
    int incomplete_count,
    int initially_selected_card_field_bitmask,
    int initially_selected_billing_address_field_bitmask,
    UserDataSelectionState selection_state) {
  ukm::builders::AutofillAssistant_CollectPayment(source_id)
      .SetCompleteCreditCardsCount(ToEntryCountBucket(complete_count))
      .SetIncompleteCreditCardsCount(ToEntryCountBucket(incomplete_count))
      .SetInitialCreditCardFieldsStatus(initially_selected_card_field_bitmask)
      .SetInitialBillingAddressFieldsStatus(
          initially_selected_billing_address_field_bitmask)
      .SetCreditCardModified(static_cast<int64_t>(selection_state))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordShippingMetrics(ukm::UkmRecorder* ukm_recorder,
                                    ukm::SourceId source_id,
                                    int complete_count,
                                    int incomplete_count,
                                    int initially_selected_field_bitmask,
                                    UserDataSelectionState selection_state) {
  ukm::builders::AutofillAssistant_CollectShippingAddress(source_id)
      .SetCompleteShippingProfilesCount(ToEntryCountBucket(complete_count))
      .SetIncompleteShippingProfilesCount(ToEntryCountBucket(incomplete_count))
      .SetInitialShippingFieldsStatus(initially_selected_field_bitmask)
      .SetShippingModified(static_cast<int64_t>(selection_state))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordCollectUserDataSuccess(ukm::UkmRecorder* ukm_recorder,
                                           ukm::SourceId source_id,
                                           CollectUserDataResult result,
                                           int64_t time_taken_ms,
                                           UserDataSource source) {
  ukm::builders::AutofillAssistant_CollectUserDataResult(source_id)
      .SetResult(static_cast<int64_t>(result))
      .SetTimeTakenMs(time_taken_ms)
      .SetUserDataSource(static_cast<int64_t>(source))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordCupRpcVerificationEvent(CupRpcVerificationEvent event) {
  DCHECK_LE(event, CupRpcVerificationEvent::kMaxValue);
  base::UmaHistogramEnumeration(kCupRpcVerificationEvent, event);
}

// static
void Metrics::RecordJsFlowStartedEvent(JsFlowStartedEvent event) {
  DCHECK_LE(event, JsFlowStartedEvent::kMaxValue);
  base::UmaHistogramEnumeration(kJsFlowStartedEvent, event);
}

// static
void Metrics::RecordOnboardingFetcherResult(
    OnboardingFetcherResultStatus status) {
  DCHECK_LE(status, OnboardingFetcherResultStatus::kMaxValue);
  base::UmaHistogramEnumeration(kOnboardingFetcherResultStatus, status);
}

void Metrics::RecordServiceRequestRetryCount(int count, bool success) {
  DCHECK_GE(count, 0);
  base::UmaHistogramExactLinear(success ? kServiceRequestSuccessRetryCount
                                        : kServiceRequestFailureRetryCount,
                                /* sample= */ count,
                                /* exclusive_max= */ 11);
}

// static
void Metrics::RecordFlowFinished(ukm::UkmRecorder* ukm_recorder,
                                 ukm::SourceId source_id,
                                 FlowFinishedState state,
                                 RoundtripNetworkStats flow_network_stats) {
  int num_js_flow_actions = 0;
  size_t total_decoded_js_flow_size_in_bytes = 0;
  for (const auto& action : flow_network_stats.action_stats()) {
    if (action.action_info_case() !=
        static_cast<int>(ActionProto::ActionInfoCase::kJsFlow)) {
      continue;
    }
    num_js_flow_actions++;
    total_decoded_js_flow_size_in_bytes += action.decoded_size_bytes();
  }

  ukm::builders::AutofillAssistant_FlowFinished(source_id)
      .SetFlowFinishedState(static_cast<int64_t>(state))
      .SetNumJsFlowActions(num_js_flow_actions)
      .SetTotalDecodedJsFlowSizeInBytes(ukm::GetExponentialBucketMinForBytes(
          total_decoded_js_flow_size_in_bytes))
      .SetNumActions(flow_network_stats.action_stats().size())
      .SetNumRoundtrips(flow_network_stats.num_roundtrips())
      .SetTotalEncodedGetActionsSizeInBytes(
          ukm::GetExponentialBucketMinForBytes(
              flow_network_stats.roundtrip_encoded_body_size_bytes()))
      .SetTotalDecodedGetActionsSizeInBytes(
          ukm::GetExponentialBucketMinForBytes(
              flow_network_stats.roundtrip_decoded_body_size_bytes()))
      .Record(ukm_recorder);
}

// static
AutofillAssistantIntent Metrics::ExtractIntentFromScriptParameters(
    const ScriptParameters& script_parameters) {
  absl::optional<std::string> intent = script_parameters.GetIntent();
  return ExtractIntentFromString(intent);
}

std::ostream& operator<<(std::ostream& out,
                         const Metrics::DropOutReason& reason) {
#ifdef NDEBUG
  // Non-debugging builds write the enum number.
  out << static_cast<int>(reason);
  return out;
#else
  // Debugging builds write a string representation of |reason|.
  switch (reason) {
    case Metrics::DropOutReason::AA_START:
      out << "AA_START";
      break;
    case Metrics::DropOutReason::AUTOSTART_TIMEOUT:
      out << "AUTOSTART_TIMEOUT";
      break;
    case Metrics::DropOutReason::NO_SCRIPTS:
      out << "NO_SCRIPTS";
      break;
    case Metrics::DropOutReason::CUSTOM_TAB_CLOSED:
      out << "CUSTOM_TAB_CLOSED";
      break;
    case Metrics::DropOutReason::DECLINED:
      out << "DECLINED";
      break;
    case Metrics::DropOutReason::SHEET_CLOSED:
      out << "SHEET_CLOSED";
      break;
    case Metrics::DropOutReason::SCRIPT_FAILED:
      out << "SCRIPT_FAILED";
      break;
    case Metrics::DropOutReason::NAVIGATION:
      out << "NAVIGATION";
      break;
    case Metrics::DropOutReason::OVERLAY_STOP:
      out << "OVERLAY_STOP";
      break;
    case Metrics::DropOutReason::PR_FAILED:
      out << "PR_FAILED";
      break;
    case Metrics::DropOutReason::CONTENT_DESTROYED:
      out << "CONTENT_DESTROYED";
      break;
    case Metrics::DropOutReason::RENDER_PROCESS_GONE:
      out << "RENDER_PROCESS_GONE";
      break;
    case Metrics::DropOutReason::INTERSTITIAL_PAGE:
      out << "INTERSTITIAL_PAGE";
      break;
    case Metrics::DropOutReason::SCRIPT_SHUTDOWN:
      out << "SCRIPT_SHUTDOWN";
      break;
    case Metrics::DropOutReason::SAFETY_NET_TERMINATE:
      out << "SAFETY_NET_TERMINATE";
      break;
    case Metrics::DropOutReason::TAB_DETACHED:
      out << "TAB_DETACHED";
      break;
    case Metrics::DropOutReason::TAB_CHANGED:
      out << "TAB_CHANGED";
      break;
    case Metrics::DropOutReason::GET_SCRIPTS_FAILED:
      out << "GET_SCRIPTS_FAILED";
      break;
    case Metrics::DropOutReason::GET_SCRIPTS_UNPARSABLE:
      out << "GET_SCRIPTS_UNPARSEABLE";
      break;
    case Metrics::DropOutReason::NO_INITIAL_SCRIPTS:
      out << "NO_INITIAL_SCRIPTS";
      break;
    case Metrics::DropOutReason::DFM_INSTALL_FAILED:
      out << "DFM_INSTALL_FAILED";
      break;
    case Metrics::DropOutReason::DOMAIN_CHANGE_DURING_BROWSE_MODE:
      out << "DOMAIN_CHANGE_DURING_BROWSE_MODE";
      break;
    case Metrics::DropOutReason::BACK_BUTTON_CLICKED:
      out << "BACK_BUTTON_CLICKED";
      break;
    case Metrics::DropOutReason::ONBOARDING_BACK_BUTTON_CLICKED:
      out << "ONBOARDING_BACK_BUTTON_CLICKED";
      break;
    case Metrics::DropOutReason::NAVIGATION_WHILE_RUNNING:
      out << "NAVIGATION_WHILE_RUNNING";
      break;
    case Metrics::DropOutReason::UI_CLOSED_UNEXPECTEDLY:
      out << "UI_CLOSED_UNEXPECTEDLY";
      break;
    case Metrics::DropOutReason::ONBOARDING_NAVIGATION:
      out << "ONBOARDING_NAVIGATION";
      break;
    case Metrics::DropOutReason::ONBOARDING_DIALOG_DISMISSED:
      out << "ONBOARDING_DIALOG_DISMISSED";
      break;
    case Metrics::DropOutReason::MULTIPLE_AUTOSTARTABLE_SCRIPTS:
      out << "MULTIPLE_AUTOSTARTABLE_SCRIPTS";
      break;
      // Do not add default case to force compilation error for new values.
  }
  return out;
#endif  // NDEBUG
}

std::ostream& operator<<(std::ostream& out, const Metrics::Onboarding& metric) {
#ifdef NDEBUG
  // Non-debugging builds write the enum number.
  out << static_cast<int>(metric);
  return out;
#else
  // Debugging builds write a string representation of |metric|.
  switch (metric) {
    case Metrics::Onboarding::OB_SHOWN:
      out << "OB_SHOWN";
      break;
    case Metrics::Onboarding::OB_NOT_SHOWN:
      out << "OB_NOT_SHOWN";
      break;
    case Metrics::Onboarding::OB_ACCEPTED:
      out << "OB_ACCEPTED";
      break;
    case Metrics::Onboarding::OB_CANCELLED:
      out << "OB_CANCELLED";
      break;
    case Metrics::Onboarding::OB_NO_ANSWER:
      out << "OB_NO_ANSWER";
      break;
    case Metrics::Onboarding::OB_EXTERNAL:
      out << "OB_EXTERNAL";
      break;
      // Do not add default case to force compilation error for new values.
  }
  return out;
#endif  // NDEBUG
}

std::ostream& operator<<(std::ostream& out,
                         const Metrics::TriggerScriptFinishedState& state) {
#ifdef NDEBUG
  // Non-debugging builds write the enum number.
  out << static_cast<int>(state);
  return out;
#else
  // Debugging builds write a string representation of |state|.
  switch (state) {
    case Metrics::TriggerScriptFinishedState::GET_ACTIONS_FAILED:
      out << "GET_ACTIONS_FAILED";
      break;
    case Metrics::TriggerScriptFinishedState::GET_ACTIONS_PARSE_ERROR:
      out << "GET_ACTIONS_PARSE_ERROR";
      break;
    case Metrics::TriggerScriptFinishedState::PROMPT_FAILED_NAVIGATE:
      out << "PROMPT_FAILED_NAVIGATE";
      break;
    case Metrics::TriggerScriptFinishedState::PROMPT_SUCCEEDED:
      out << "PROMPT_SUCCEEDED";
      break;
    case Metrics::TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_SESSION:
      out << "PROMPT_FAILED_CANCEL_SESSION";
      break;
    case Metrics::TriggerScriptFinishedState::PROMPT_FAILED_CANCEL_FOREVER:
      out << "PROMPT_FAILED_CANCEL_FOREVER";
      break;
    case Metrics::TriggerScriptFinishedState::TRIGGER_CONDITION_TIMEOUT:
      out << "TRIGGER_CONDITION_TIMEOUT";
      break;
    case Metrics::TriggerScriptFinishedState::NAVIGATION_ERROR:
      out << "NAVIGATION_ERROR";
      break;
    case Metrics::TriggerScriptFinishedState::
        WEB_CONTENTS_DESTROYED_WHILE_VISIBLE:
      out << "WEB_CONTENTS_DESTROYED_WHILE_VISIBLE";
      break;
    case Metrics::TriggerScriptFinishedState::
        WEB_CONTENTS_DESTROYED_WHILE_INVISIBLE:
      out << "WEB_CONTENTS_DESTROYED_WHILE_INVISIBLE";
      break;
    case Metrics::TriggerScriptFinishedState::NO_TRIGGER_SCRIPT_AVAILABLE:
      out << "NO_TRIGGER_SCRIPT_AVAILABLE";
      break;
    case Metrics::TriggerScriptFinishedState::FAILED_TO_SHOW:
      out << "FAILED_TO_SHOW";
      break;
    case Metrics::TriggerScriptFinishedState::DISABLED_PROACTIVE_HELP_SETTING:
      out << "DISABLED_PROACTIVE_HELP_SETTING";
      break;
    case Metrics::TriggerScriptFinishedState::BASE64_DECODING_ERROR:
      out << "BASE64 DECODING ERROR";
      break;
    case Metrics::TriggerScriptFinishedState::BOTTOMSHEET_ONBOARDING_REJECTED:
      out << "BOTTOMSHEET_ONBOARDING_REJECTED";
      break;
    case Metrics::TriggerScriptFinishedState::UNKNOWN_FAILURE:
      out << "UNKNOWN_FAILURE";
      break;
    case Metrics::TriggerScriptFinishedState::SERVICE_DELETED:
      out << "SERVICE_DELETED";
      break;
    case Metrics::TriggerScriptFinishedState::PATH_MISMATCH:
      out << "PATH_MISMATCH";
      break;
    case Metrics::TriggerScriptFinishedState::UNSAFE_ACTIONS:
      out << "UNSAFE_ACTIONS";
      break;
    case Metrics::TriggerScriptFinishedState::INVALID_SCRIPT:
      out << "INVALID_SCRIPT";
      break;
    case Metrics::TriggerScriptFinishedState::BROWSE_FAILED_NAVIGATE:
      out << "BROWSE_FAILED_NAVIGATE";
      break;
    case Metrics::TriggerScriptFinishedState::BROWSE_FAILED_OTHER:
      out << "BROWSE_FAILED_OTHER";
      break;
    case Metrics::TriggerScriptFinishedState::
        PROMPT_FAILED_CONDITION_NO_LONGER_TRUE:
      out << "PROMPT_FAILED_CONDITION_NO_LONGER_TRUE";
      break;
    case Metrics::TriggerScriptFinishedState::PROMPT_FAILED_CLOSE:
      out << "PROMPT_FAILED_CLOSE";
      break;
    case Metrics::TriggerScriptFinishedState::PROMPT_FAILED_OTHER:
      out << "PROMPT_FAILED_OTHER";
      break;
    case Metrics::TriggerScriptFinishedState::PROMPT_SWIPE_DISMISSED:
      out << "PROMPT_SWIPE_DISMISSED";
      break;
    case Metrics::TriggerScriptFinishedState::CCT_TO_TAB_NOT_SUPPORTED:
      out << "CCT_TO_TAB_NOT_SUPPORTED";
      break;
    case Metrics::TriggerScriptFinishedState::CANCELED:
      out << "CANCELED";
      break;
      // Do not add default case to force compilation error for new values.
  }
  return out;
#endif  // NDEBUG
}

}  // namespace autofill_assistant
