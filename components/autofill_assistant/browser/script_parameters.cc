// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_parameters.h"

#include <array>
#include <sstream>
#include "base/logging.h"

namespace {

// Converts a value to a target type. Returns nullopt for invalid or
// non-existent values. Expects bool parameters as 'false' and 'true'.
template <typename T>
base::Optional<T> GetTypedParameter(
    const std::map<std::string, std::string> parameters,
    const std::string& key) {
  auto iter = parameters.find(key);
  if (iter == parameters.end())
    return base::nullopt;

  std::stringstream ss;
  ss << iter->second;
  T out;
  if (!(ss >> std::boolalpha >> out)) {
    LOG(ERROR) << "Error trying to convert parameter '" << key
               << "' with value '" << iter->second << "' to target type";
    return base::nullopt;
  }
  return out;
}

}  // namespace

namespace autofill_assistant {

// Parameter that allows setting the color of the overlay.
const char kOverlayColorParameterName[] = "OVERLAY_COLORS";

// Parameter that contains the current session username. Should be synced with
// |SESSION_USERNAME_PARAMETER| from
// .../password_manager/PasswordChangeLauncher.java
// TODO(b/151401974): Eliminate duplicate parameter definitions.
const char kPasswordChangeUsernameParameterName[] = "PASSWORD_CHANGE_USERNAME";

// Parameter that contains a base64-encoded GetTriggerScriptsResponseProto
// message. Instructs the client to decode and run this trigger script prior to
// starting the regular flow. Takes precedence over REQUEST_TRIGGER_SCRIPT if
// both are specified.
const char kBase64TriggerScriptsResponseProtoParameterName[] =
    "TRIGGER_SCRIPTS_BASE64";

// Special parameter for instructing the client to request and run a trigger
// script from a remote RPC prior to starting the regular flow.
const char kRequestTriggerScriptParameterName[] = "REQUEST_TRIGGER_SCRIPT";

// Special bool parameter that MUST be present in all intents. It allows the
// caller to either request immediate start of autofill assistant (if set to
// true), or a delayed start using trigger scripts (if set to false). If this is
// set to false, REQUEST_TRIGGER_SCRIPT or TRIGGER_SCRIPTS_BASE_64 must be set.
const char kStartImmediatelyParameterName[] = "START_IMMEDIATELY";

// Mandatory parameter that MUST be present and set to true in all intents.
const char kEnabledParameterName[] = "ENABLED";

// The intent parameter.
const char kIntent[] = "INTENT";

// The list of script parameters that trigger scripts are allowed to send to
// the backend.
constexpr std::array<const char*, 5> kAllowlistedTriggerScriptParameters = {
    "DEBUG_BUNDLE_ID", "DEBUG_BUNDLE_VERSION", "DEBUG_SOCKET_ID",
    "FALLBACK_BUNDLE_ID", "FALLBACK_BUNDLE_VERSION"};

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

ScriptParameters::ScriptParameters(
    const std::map<std::string, std::string>& parameters)
    : parameters_(parameters) {}

ScriptParameters::ScriptParameters() = default;
ScriptParameters::~ScriptParameters() = default;

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
ScriptParameters::ToProto(bool only_trigger_script_allowlisted) const {
  google::protobuf::RepeatedPtrField<ScriptParameterProto> out;
  if (only_trigger_script_allowlisted) {
    for (const char* key : kAllowlistedTriggerScriptParameters) {
      auto iter = parameters_.find(key);
      if (iter == parameters_.end()) {
        continue;
      }
      auto* out_param = out.Add();
      out_param->set_name(key);
      out_param->set_value(iter->second);
    }
    return out;
  }

  // TODO(arbesser): Send properly typed parameters to backend.
  for (const auto& parameter : parameters_) {
    auto* out_param = out.Add();
    out_param->set_name(parameter.first);
    out_param->set_value(parameter.second);
  }
  return out;
}

base::Optional<std::string> ScriptParameters::GetParameter(
    const std::string& name) const {
  auto iter = parameters_.find(name);
  if (iter == parameters_.end())
    return base::nullopt;

  return iter->second;
}

base::Optional<std::string> ScriptParameters::GetOverlayColors() const {
  return GetParameter(kOverlayColorParameterName);
}

base::Optional<std::string> ScriptParameters::GetPasswordChangeUsername()
    const {
  return GetParameter(kPasswordChangeUsernameParameterName);
}

base::Optional<std::string>
ScriptParameters::GetBase64TriggerScriptsResponseProto() const {
  return GetParameter(kBase64TriggerScriptsResponseProtoParameterName);
}

base::Optional<bool> ScriptParameters::GetRequestsTriggerScript() const {
  return GetTypedParameter<bool>(parameters_,
                                 kRequestTriggerScriptParameterName);
}

base::Optional<bool> ScriptParameters::GetStartImmediately() const {
  return GetTypedParameter<bool>(parameters_, kStartImmediatelyParameterName);
}

base::Optional<bool> ScriptParameters::GetEnabled() const {
  return GetTypedParameter<bool>(parameters_, kEnabledParameterName);
}

base::Optional<std::string> ScriptParameters::GetIntent() const {
  return GetParameter(kIntent);
}

base::Optional<bool> ScriptParameters::GetDetailsShowInitial() const {
  return GetTypedParameter<bool>(parameters_, kDetailsShowInitialParameterName);
}

base::Optional<std::string> ScriptParameters::GetDetailsTitle() const {
  return GetParameter(kDetailsTitleParameterName);
}

base::Optional<std::string> ScriptParameters::GetDetailsDescriptionLine1()
    const {
  return GetParameter(kDetailsDescriptionLine1ParameterName);
}

base::Optional<std::string> ScriptParameters::GetDetailsDescriptionLine2()
    const {
  return GetParameter(kDetailsDescriptionLine2ParameterName);
}

base::Optional<std::string> ScriptParameters::GetDetailsDescriptionLine3()
    const {
  return GetParameter(kDetailsDescriptionLine3ParameterName);
}

base::Optional<std::string> ScriptParameters::GetDetailsImageUrl() const {
  return GetParameter(kDetailsImageUrl);
}

base::Optional<std::string> ScriptParameters::GetDetailsImageAccessibilityHint()
    const {
  return GetParameter(kDetailsImageAccessibilityHint);
}

base::Optional<std::string> ScriptParameters::GetDetailsImageClickthroughUrl()
    const {
  return GetParameter(kDetailsImageClickthroughUrl);
}

base::Optional<std::string> ScriptParameters::GetDetailsTotalPriceLabel()
    const {
  return GetParameter(kDetailsTotalPriceLabel);
}

base::Optional<std::string> ScriptParameters::GetDetailsTotalPrice() const {
  return GetParameter(kDetailsTotalPrice);
}

}  // namespace autofill_assistant
