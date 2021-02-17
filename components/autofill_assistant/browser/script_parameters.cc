// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_parameters.h"

#include <array>

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

// The list of script parameters that trigger scripts are allowed to send to
// the backend.
constexpr std::array<const char*, 5> kAllowlistedTriggerScriptParameters = {
    "DEBUG_BUNDLE_ID", "DEBUG_BUNDLE_VERSION", "DEBUG_SOCKET_ID",
    "FALLBACK_BUNDLE_ID", "FALLBACK_BUNDLE_VERSION"};

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

}  // namespace autofill_assistant
