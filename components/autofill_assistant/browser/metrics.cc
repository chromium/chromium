// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/metrics.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/intent_strings.h"
#include "components/ukm/content/source_url_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"

#include <map>

namespace autofill_assistant {

// Intent not set constant.
const char* const kIntentNotSet = "NotSet";

namespace {
const char kDropOutEnumName[] = "Android.AutofillAssistant.DropOutReason";
const char kOnboardingEnumName[] = "Android.AutofillAssistant.OnBoarding";
const char kPaymentRequestPrefilledName[] =
    "Android.AutofillAssistant.PaymentRequest.Prefilled";
const char kPaymentRequestAutofillInfoChangedName[] =
    "Android.AutofillAssistant.PaymentRequest.AutofillChanged";
const char kPaymentRequestFirstNameOnly[] =
    "Android.AutofillAssistant.PaymentRequest.FirstNameOnly";
const char kPaymentRequestMandatoryPostalCode[] =
    "Android.AutofillAssistant.PaymentRequest.MandatoryPostalCode";
static bool DROPOUT_RECORDED = false;

std::string GetSuffixForIntent(const std::string& intent) {
  std::map<std::string, std::string> histogramsSuffixes = {
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
  base::UmaHistogramEnumeration(kDropOutEnumName + suffix, reason);
  base::UmaHistogramEnumeration(kDropOutEnumName, reason);
  if (reason != DropOutReason::AA_START) {
    DVLOG(3) << "Drop out with reason: " << reason;
    DROPOUT_RECORDED = true;
  }
}

// static
void Metrics::RecordPaymentRequestPrefilledSuccess(bool initially_complete,
                                                   bool success) {
  if (initially_complete && success) {
    base::UmaHistogramEnumeration(kPaymentRequestPrefilledName,
                                  PaymentRequestPrefilled::PREFILLED_SUCCESS);
  } else if (initially_complete && !success) {
    base::UmaHistogramEnumeration(kPaymentRequestPrefilledName,
                                  PaymentRequestPrefilled::PREFILLED_FAILURE);
  } else if (!initially_complete && success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestPrefilledName,
        PaymentRequestPrefilled::NOTPREFILLED_SUCCESS);
  } else if (!initially_complete && !success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestPrefilledName,
        PaymentRequestPrefilled::NOTPREFILLED_FAILURE);
  }
}

// static
void Metrics::RecordPaymentRequestAutofillChanged(bool changed, bool success) {
  if (changed && success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestAutofillInfoChangedName,
        PaymentRequestAutofillInfoChanged::CHANGED_SUCCESS);
  } else if (changed && !success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestAutofillInfoChangedName,
        PaymentRequestAutofillInfoChanged::CHANGED_FAILURE);
  } else if (!changed && success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestAutofillInfoChangedName,
        PaymentRequestAutofillInfoChanged::NOTCHANGED_SUCCESS);
  } else if (!changed && !success) {
    base::UmaHistogramEnumeration(
        kPaymentRequestAutofillInfoChangedName,
        PaymentRequestAutofillInfoChanged::NOTCHANGED_FAILURE);
  }
}

// static
void Metrics::RecordPaymentRequestFirstNameOnly(bool first_name_only) {
  base::UmaHistogramBoolean(kPaymentRequestFirstNameOnly, first_name_only);
}

// static
void Metrics::RecordPaymentRequestMandatoryPostalCode(bool required,
                                                      bool initially_right,
                                                      bool success) {
  PaymentRequestMandatoryPostalCode mandatory_postal_code;
  if (!required) {
    mandatory_postal_code = PaymentRequestMandatoryPostalCode::NOT_REQUIRED;
  } else if (initially_right && success) {
    mandatory_postal_code =
        PaymentRequestMandatoryPostalCode::REQUIRED_INITIALLY_RIGHT_SUCCESS;
  } else if (initially_right && !success) {
    mandatory_postal_code =
        PaymentRequestMandatoryPostalCode::REQUIRED_INITIALLY_RIGHT_FAILURE;
  } else if (!initially_right && success) {
    mandatory_postal_code =
        PaymentRequestMandatoryPostalCode::REQUIRED_INITIALLY_WRONG_SUCCESS;
  } else if (!initially_right && !success) {
    mandatory_postal_code =
        PaymentRequestMandatoryPostalCode::REQUIRED_INITIALLY_WRONG_FAILURE;
  } else {
    DCHECK(false) << "Not reached";
    return;
  }

  base::UmaHistogramEnumeration(kPaymentRequestMandatoryPostalCode,
                                mandatory_postal_code);
}

// static
void Metrics::RecordLiteScriptStarted(ukm::UkmRecorder* ukm_recorder,
                                      content::WebContents* web_contents,
                                      StartupUtil::StartupMode startup_mode,
                                      bool feature_module_installed,
                                      bool is_first_time_user) {
  LiteScriptStarted event;
  switch (startup_mode) {
    case StartupUtil::StartupMode::FEATURE_DISABLED:
      if (base::FeatureList::IsEnabled(
              features::kAutofillAssistantProactiveHelp) &&
          !feature_module_installed) {
        event = LiteScriptStarted::LITE_SCRIPT_DFM_UNAVAILABLE;
      } else {
        event = LiteScriptStarted::LITE_SCRIPT_FEATURE_DISABLED;
      }
      break;
    case StartupUtil::StartupMode::SETTING_DISABLED:
      event = LiteScriptStarted::LITE_SCRIPT_PROACTIVE_TRIGGERING_DISABLED;
      break;
    case StartupUtil::StartupMode::NO_INITIAL_URL:
      event = LiteScriptStarted::LITE_SCRIPT_NO_INITIAL_URL;
      break;
    case StartupUtil::StartupMode::MANDATORY_PARAMETERS_MISSING:
      event = LiteScriptStarted::LITE_SCRIPT_MANDATORY_PARAMETER_MISSING;
      break;
    case StartupUtil::StartupMode::START_BASE64_TRIGGER_SCRIPT:
    case StartupUtil::StartupMode::START_RPC_TRIGGER_SCRIPT:
      event = is_first_time_user
                  ? LiteScriptStarted::LITE_SCRIPT_FIRST_TIME_USER
                  : LiteScriptStarted::LITE_SCRIPT_RETURNING_USER;
      break;
    case StartupUtil::StartupMode::START_REGULAR:
      // Regular starts do not record impressions for |LiteScriptStarted|.
      return;
  }

  ukm::builders::AutofillAssistant_LiteScriptStarted(
      ukm::GetSourceIdForWebContentsDocument(web_contents))
      .SetLiteScriptStarted(static_cast<int64_t>(event))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordLiteScriptFinished(ukm::UkmRecorder* ukm_recorder,
                                       content::WebContents* web_contents,
                                       TriggerUIType trigger_ui_type,
                                       LiteScriptFinishedState event) {
  ukm::builders::AutofillAssistant_LiteScriptFinished(
      ukm::GetSourceIdForWebContentsDocument(web_contents))
      .SetTriggerUIType(static_cast<int64_t>(trigger_ui_type))
      .SetLiteScriptFinished(static_cast<int64_t>(event))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordLiteScriptShownToUser(ukm::UkmRecorder* ukm_recorder,
                                          content::WebContents* web_contents,
                                          TriggerUIType trigger_ui_type,
                                          LiteScriptShownToUser event) {
  ukm::builders::AutofillAssistant_LiteScriptShownToUser(
      ukm::GetSourceIdForWebContentsDocument(web_contents))
      .SetTriggerUIType(static_cast<int64_t>(trigger_ui_type))
      .SetLiteScriptShownToUser(static_cast<int64_t>(event))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordLiteScriptOnboarding(ukm::UkmRecorder* ukm_recorder,
                                         content::WebContents* web_contents,
                                         TriggerUIType trigger_ui_type,
                                         LiteScriptOnboarding event) {
  ukm::builders::AutofillAssistant_LiteScriptOnboarding(
      ukm::GetSourceIdForWebContentsDocument(web_contents))
      .SetTriggerUIType(static_cast<int64_t>(trigger_ui_type))
      .SetLiteScriptOnboarding(static_cast<int64_t>(event))
      .Record(ukm_recorder);
}

// static
void Metrics::RecordOnboardingResult(OnBoarding event) {
  DCHECK_LE(event, OnBoarding::kMaxValue);
  base::UmaHistogramEnumeration(kOnboardingEnumName, event);
}

}  // namespace autofill_assistant
