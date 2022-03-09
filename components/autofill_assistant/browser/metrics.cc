// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/metrics.h"

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/intent_strings.h"
#include "components/ukm/content/source_url_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"

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
const char kDependenciesInvalidated[] =
    "Android.AutofillAssistant.DependenciesInvalidated";
const char kOnboardingFetcherResultStatus[] =
    "Android.AutofillAssistant.OnboardingFetcher.ResultStatus";
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

// Extracts the enum value corresponding to the intent specified in
// |script_parameters|.
Metrics::AutofillAssistantIntent ExtractIntentFromScriptParameters(
    const ScriptParameters& script_parameters) {
  auto intent = script_parameters.GetIntent();
  if (!intent) {
    return Metrics::AutofillAssistantIntent::UNDEFINED_INTENT;
  }
  // The list of intents that is known at compile-time. Intents not in this list
  // will be recorded as UNDEFINED_INTENT.
  static const base::NoDestructor<
      base::flat_map<std::string, Metrics::AutofillAssistantIntent>>
      intents(
          {{"BUY_MOVIE_TICKET",
            Metrics::AutofillAssistantIntent::BUY_MOVIE_TICKET},
           {"RENT_CAR", Metrics::AutofillAssistantIntent::RENT_CAR},
           {"SHOPPING", Metrics::AutofillAssistantIntent::SHOPPING},
           {"TELEPORT", Metrics::AutofillAssistantIntent::TELEPORT},
           {"SHOPPING_ASSISTED_CHECKOUT",
            Metrics::AutofillAssistantIntent::SHOPPING_ASSISTED_CHECKOUT},
           {"FLIGHTS_CHECKIN",
            Metrics::AutofillAssistantIntent::FLIGHTS_CHECKIN},
           {"FOOD_ORDERING", Metrics::AutofillAssistantIntent::FOOD_ORDERING},
           {"PASSWORD_CHANGE",
            Metrics::AutofillAssistantIntent::PASSWORD_CHANGE},
           {"FOOD_ORDERING_PICKUP",
            Metrics::AutofillAssistantIntent::FOOD_ORDERING_PICKUP},
           {"FOOD_ORDERING_DELIVERY",
            Metrics::AutofillAssistantIntent::FOOD_ORDERING_DELIVERY},
           {"UNLAUNCHED_VERTICAL_1",
            Metrics::AutofillAssistantIntent::UNLAUNCHED_VERTICAL_1},
           {"FIND_COUPONS", Metrics::AutofillAssistantIntent::FIND_COUPONS}});

  auto enum_value_iter = intents->find(*intent);
  if (enum_value_iter == intents->end()) {
    return Metrics::AutofillAssistantIntent::UNDEFINED_INTENT;
  }
  return enum_value_iter->second;
}  // namespace

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
    StartupUtil::StartupMode event) {
  switch (event) {
    case StartupUtil::StartupMode::FEATURE_DISABLED:
      return Metrics::AutofillAssistantStarted::FAILED_FEATURE_DISABLED;
    case StartupUtil::StartupMode::MANDATORY_PARAMETERS_MISSING:
      return Metrics::AutofillAssistantStarted::
          FAILED_MANDATORY_PARAMETER_MISSING;
    case StartupUtil::StartupMode::SETTING_DISABLED:
      return Metrics::AutofillAssistantStarted::FAILED_SETTING_DISABLED;
    case StartupUtil::StartupMode::NO_INITIAL_URL:
      return Metrics::AutofillAssistantStarted::FAILED_NO_INITIAL_URL;
    case StartupUtil::StartupMode::START_REGULAR:
      return Metrics::AutofillAssistantStarted::OK_IMMEDIATE_START;
    case StartupUtil::StartupMode::START_BASE64_TRIGGER_SCRIPT:
    case StartupUtil::StartupMode::START_RPC_TRIGGER_SCRIPT:
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
                                         StartupUtil::StartupMode startup_mode,
                                         bool feature_module_installed,
                                         bool is_first_time_user) {
  TriggerScriptStarted event;
  switch (startup_mode) {
    case StartupUtil::StartupMode::FEATURE_DISABLED:
      if (base::FeatureList::IsEnabled(
              features::kAutofillAssistantProactiveHelp) &&
          !feature_module_installed) {
        event = TriggerScriptStarted::DFM_UNAVAILABLE;
      } else {
        event = TriggerScriptStarted::FEATURE_DISABLED;
      }
      break;
    case StartupUtil::StartupMode::SETTING_DISABLED:
      event = TriggerScriptStarted::PROACTIVE_TRIGGERING_DISABLED;
      break;
    case StartupUtil::StartupMode::NO_INITIAL_URL:
      event = TriggerScriptStarted::NO_INITIAL_URL;
      break;
    case StartupUtil::StartupMode::MANDATORY_PARAMETERS_MISSING:
      event = TriggerScriptStarted::MANDATORY_PARAMETER_MISSING;
      break;
    case StartupUtil::StartupMode::START_BASE64_TRIGGER_SCRIPT:
    case StartupUtil::StartupMode::START_RPC_TRIGGER_SCRIPT:
      event = is_first_time_user ? TriggerScriptStarted::FIRST_TIME_USER
                                 : TriggerScriptStarted::RETURNING_USER;
      break;
    case StartupUtil::StartupMode::START_REGULAR:
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
                                 StartupUtil::StartupMode event) {
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
void Metrics::RecordOnboardingFetcherResult(
    OnboardingFetcherResultStatus status) {
  DCHECK_LE(status, OnboardingFetcherResultStatus::kMaxValue);
  base::UmaHistogramEnumeration(kOnboardingFetcherResultStatus, status);
}

}  // namespace autofill_assistant
