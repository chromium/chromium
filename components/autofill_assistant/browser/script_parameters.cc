// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_parameters.h"

#include <array>
#include <sstream>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "components/autofill_assistant/browser/public/public_script_parameters.h"
#include "components/autofill_assistant/browser/user_data.h"
#include "components/autofill_assistant/browser/value_util.h"

namespace autofill_assistant {
namespace {

// Prefix used to annotate values coming from the the startup parameters.
const char kParameterMemoryPrefix[] = "param:";

// Converts a value to a target type. Returns nullopt for invalid or
// non-existent values. Expects bool parameters as 'false' and 'true'.
template <typename T>
absl::optional<T> GetTypedParameter(
    const base::flat_map<std::string, ValueProto>& parameters,
    const std::string& key) {
  auto iter = parameters.find(key);
  if (iter == parameters.end())
    return absl::nullopt;

  std::string value = iter->second.strings().values(0);
  std::stringstream ss;
  ss << value;
  T out;
  if (!(ss >> std::boolalpha >> out)) {
    LOG(ERROR) << "Error trying to convert parameter '" << key
               << "' with value '" << value << "' to target type";
    return absl::nullopt;
  }
  return out;
}

bool GetBoolParameter(const base::flat_map<std::string, ValueProto>& parameters,
                      const std::string& key) {
  return GetTypedParameter<bool>(parameters, key).value_or(false);
}

}  // namespace

// Parameter that allows setting the color of the overlay.
const char kOverlayColorParameterName[] = "OVERLAY_COLORS";

// Special parameter for instructing the client to request and run a trigger
// script from a remote RPC prior to starting the regular flow.
const char kRequestTriggerScriptParameterName[] = "REQUEST_TRIGGER_SCRIPT";

// The parameter key for the user's email, as indicated by the caller.
const char kCallerEmailParameterName[] = "USER_EMAIL";

// Special parameter for declaring a user to be in a trigger script experiment.
const char kTriggerScriptExperimentParameterName[] =
    "TRIGGER_SCRIPT_EXPERIMENT";

// Parameter that allows enabling Text-to-Speech functionality.
const char kEnableTtsParameterName[] = "ENABLE_TTS";

// Allows enabling observer-based WaitForDOM.
const char kEnableObserversParameter[] = "ENABLE_OBSERVER_WAIT_FOR_DOM";

// Parameter to specify experiments.
const char kExperimentsParameterName[] = "EXPERIMENT_IDS";

// Parameter to send the annotate DOM model version. Should only be used if we
// expect the model to be used.
const char kSendAnnotateDomModelVersion[] = "SEND_ANNOTATE_DOM_MODEL_VERSION";

// Whether this script does not requires a round trip.
const char kIsNoRoundTrip[] = "IS_NO_ROUND_TRIP";

// The list of non sensitive script parameters that client requests are allowed
// to send to the backend i.e., they do not require explicit approval in the
// autofill-assistant onboarding. Even so, please always reach out to Chrome
// privacy when you plan to make use of this list, and/or adjust it.
constexpr std::array<const char*, 7> kNonSensitiveScriptParameters = {
    public_script_parameters::kDebugBundleIdParameterName,
    "DEBUG_BUNDLE_VERSION",
    public_script_parameters::kDebugSocketIdParameterName,
    "FALLBACK_BUNDLE_ID",
    "FALLBACK_BUNDLE_VERSION",
    public_script_parameters::kIntentParameterName,
    "CAPABILITIES_REQUEST_ID"};

// Parameters to specify details before the first backend roundtrip.
const char kDetailsShowInitialParameterName[] = "DETAILS_SHOW_INITIAL";
const char kDetailsTitleParameterName[] = "DETAILS_TITLE";
const char kDetailsDescriptionLine1ParameterName[] =
    "DETAILS_DESCRIPTION_LINE_1";
const char kDetailsDescriptionLine2ParameterName[] =
    "DETAILS_DESCRIPTION_LINE_2";
const char kDetailsDescriptionLine3ParameterName[] =
    "DETAILS_DESCRIPTION_LINE_3";
const char kDetailsImageUrl[] = "DETAILS_IMAGE_URL";
const char kDetailsImageAccessibilityHint[] =
    "DETAILS_IMAGE_ACCESSIBILITY_HINT";
const char kDetailsImageClickthroughUrl[] = "DETAILS_IMAGE_CLICKTHROUGH_URL";
const char kDetailsTotalPriceLabel[] = "DETAILS_TOTAL_PRICE_LABEL";
const char kDetailsTotalPrice[] = "DETAILS_TOTAL_PRICE";
const char kRunHeadless[] = "RUN_HEADLESS";
const char kFieldTrialPrefix[] = "FIELD_TRIAL_";
const char kUseAssistantUi[] = "USE_ASSISTANT_UI";

ScriptParameters::ScriptParameters(
    const base::flat_map<std::string, std::string>& parameters) {
  for (const auto& it : parameters) {
    parameters_.emplace(
        it.first, SimpleValue(it.second, /* is_client_side_only= */ false));
  }
}

ScriptParameters::ScriptParameters() = default;
ScriptParameters::~ScriptParameters() = default;
ScriptParameters& ScriptParameters::operator=(ScriptParameters&&) = default;

void ScriptParameters::MergeWith(const ScriptParameters& another) {
  for (const auto& param : another.parameters_) {
    parameters_.insert(param);
  }
}

bool ScriptParameters::Matches(const ScriptParameterMatchProto& proto) const {
  auto opt_value = GetParameter(proto.name());
  if (!proto.exists()) {
    return !opt_value;
  }

  if (!proto.has_value_equals()) {
    return opt_value.has_value();
  }

  return opt_value && proto.value_equals() == opt_value.value();
}

google::protobuf::RepeatedPtrField<ScriptParameterProto>
ScriptParameters::ToProto(bool only_non_sensitive_allowlisted) const {
  google::protobuf::RepeatedPtrField<ScriptParameterProto> out;
  if (only_non_sensitive_allowlisted) {
    for (const char* key : kNonSensitiveScriptParameters) {
      auto iter = parameters_.find(key);
      if (iter == parameters_.end()) {
        continue;
      }
      auto* out_param = out.Add();
      out_param->set_name(key);
      out_param->set_value(iter->second.strings().values(0));
    }
    return out;
  }

  // TODO(arbesser): Send properly typed parameters to backend.
  for (const auto& parameter : parameters_) {
    if (parameter.first == public_script_parameters::kEnabledParameterName) {
      continue;
    }
    if (parameter.second.is_client_side_only()) {
      continue;
    }
    auto* out_param = out.Add();
    out_param->set_name(parameter.first);
    out_param->set_value(parameter.second.strings().values(0));
  }
  return out;
}

absl::optional<std::string> ScriptParameters::GetParameter(
    const std::string& name) const {
  auto iter = parameters_.find(name);
  if (iter == parameters_.end())
    return absl::nullopt;

  return iter->second.strings().values(0);
}

bool ScriptParameters::HasExperimentId(const std::string& experiment_id) const {
  return base::ranges::count(GetExperiments(), experiment_id) > 0;
}

absl::optional<std::string> ScriptParameters::GetOverlayColors() const {
  return GetParameter(kOverlayColorParameterName);
}

absl::optional<std::string> ScriptParameters::GetPasswordChangeUsername()
    const {
  return GetParameter(
      public_script_parameters::kPasswordChangeUsernameParameterName);
}

bool ScriptParameters::GetRequestsTriggerScript() const {
  return GetBoolParameter(parameters_, kRequestTriggerScriptParameterName);
}

bool ScriptParameters::GetStartImmediately() const {
  return GetBoolParameter(
      parameters_, public_script_parameters::kStartImmediatelyParameterName);
}

bool ScriptParameters::HasStartImmediately() const {
  return GetTypedParameter<bool>(
             parameters_,
             public_script_parameters::kStartImmediatelyParameterName)
      .has_value();
}

bool ScriptParameters::GetEnabled() const {
  return GetBoolParameter(parameters_,
                          public_script_parameters::kEnabledParameterName);
}

absl::optional<std::string> ScriptParameters::GetOriginalDeeplink() const {
  return GetParameter(public_script_parameters::kOriginalDeeplinkParameterName);
}

bool ScriptParameters::GetTriggerScriptExperiment() const {
  return GetBoolParameter(parameters_, kTriggerScriptExperimentParameterName);
}

absl::optional<std::string> ScriptParameters::GetIntent() const {
  return GetParameter(public_script_parameters::kIntentParameterName);
}

absl::optional<std::string> ScriptParameters::GetCallerEmail() const {
  return GetParameter(kCallerEmailParameterName);
}

bool ScriptParameters::GetEnableTts() const {
  return GetBoolParameter(parameters_, kEnableTtsParameterName);
}

bool ScriptParameters::GetEnableObserverWaitForDom() const {
  return GetBoolParameter(parameters_, kEnableObserversParameter);
}

absl::optional<int> ScriptParameters::GetCaller() const {
  return GetTypedParameter<int>(parameters_,
                                public_script_parameters::kCallerParameterName);
}

absl::optional<int> ScriptParameters::GetSource() const {
  return GetTypedParameter<int>(parameters_,
                                public_script_parameters::kSourceParameterName);
}

std::vector<std::string> ScriptParameters::GetExperiments() const {
  absl::optional<std::string> experiments_str =
      GetParameter(kExperimentsParameterName);
  if (!experiments_str) {
    return std::vector<std::string>();
  }

  return base::SplitString(*experiments_str, ",",
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

bool ScriptParameters::GetDisableRpcSigning() const {
  return GetBoolParameter(
      parameters_, public_script_parameters::kDisableRpcSigningParameterName);
}

bool ScriptParameters::GetSendAnnotateDomModelVersion() const {
  return GetBoolParameter(parameters_, kSendAnnotateDomModelVersion);
}

bool ScriptParameters::GetRunHeadless() const {
  return GetBoolParameter(parameters_, kRunHeadless);
}

bool ScriptParameters::GetUseAssistantUi() const {
  return GetBoolParameter(parameters_, kUseAssistantUi);
}

absl::optional<std::string> ScriptParameters::GetFieldTrialGroup(
    const int field_trial_slot) const {
  DCHECK_GE(field_trial_slot, 1);
  DCHECK_LE(field_trial_slot,
            AssistantFieldTrialUtil::kSyntheticTrialParamCount);
  return GetParameter(base::StrCat(
      {kFieldTrialPrefix, base::NumberToString(field_trial_slot)}));
}

bool ScriptParameters::HasDetailsShowInitial() const {
  return GetTypedParameter<bool>(parameters_, kDetailsShowInitialParameterName)
      .has_value();
}

bool ScriptParameters::GetDetailsShowInitial() const {
  return GetBoolParameter(parameters_, kDetailsShowInitialParameterName);
}

absl::optional<std::string> ScriptParameters::GetDetailsTitle() const {
  return GetParameter(kDetailsTitleParameterName);
}

absl::optional<std::string> ScriptParameters::GetDetailsDescriptionLine1()
    const {
  return GetParameter(kDetailsDescriptionLine1ParameterName);
}

absl::optional<std::string> ScriptParameters::GetDetailsDescriptionLine2()
    const {
  return GetParameter(kDetailsDescriptionLine2ParameterName);
}

absl::optional<std::string> ScriptParameters::GetDetailsDescriptionLine3()
    const {
  return GetParameter(kDetailsDescriptionLine3ParameterName);
}

absl::optional<std::string> ScriptParameters::GetDetailsImageUrl() const {
  return GetParameter(kDetailsImageUrl);
}

absl::optional<std::string> ScriptParameters::GetDetailsImageAccessibilityHint()
    const {
  return GetParameter(kDetailsImageAccessibilityHint);
}

absl::optional<std::string> ScriptParameters::GetDetailsImageClickthroughUrl()
    const {
  return GetParameter(kDetailsImageClickthroughUrl);
}

absl::optional<std::string> ScriptParameters::GetDetailsTotalPriceLabel()
    const {
  return GetParameter(kDetailsTotalPriceLabel);
}

absl::optional<std::string> ScriptParameters::GetDetailsTotalPrice() const {
  return GetParameter(kDetailsTotalPrice);
}

absl::optional<bool> ScriptParameters::GetIsNoRoundtrip() const {
  return GetTypedParameter<bool>(parameters_, kIsNoRoundTrip);
}

void ScriptParameters::UpdateDeviceOnlyParameters(
    const base::flat_map<std::string, std::string>& parameters) {
  for (const auto& parameter : parameters) {
    parameters_[parameter.first] =
        SimpleValue(parameter.second, /* is_client_side_only= */ true);
  }
}

void ScriptParameters::WriteToUserData(UserData* user_data) const {
  for (const auto& parameter : parameters_) {
    user_data->SetAdditionalValue(kParameterMemoryPrefix + parameter.first,
                                  parameter.second);
  }
}

}  // namespace autofill_assistant
