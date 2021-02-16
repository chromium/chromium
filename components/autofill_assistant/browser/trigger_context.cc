// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_context.h"
#include "base/strings/string_split.h"

namespace autofill_assistant {

// Parameter that allows setting the color of the overlay.
const char kOverlayColorParameterName[] = "OVERLAY_COLORS";

// Parameter that contains the current session username. Should be synced with
// |SESSION_USERNAME_PARAMETER| from
// .../password_manager/PasswordChangeLauncher.java
// TODO(b/151401974): Eliminate duplicate parameter definitions.
const char kPasswordChangeUsernameParameterName[] = "PASSWORD_CHANGE_USERNAME";

// Parameter that contains a base64-encoded GetTriggerScriptsResponseProto
// message. This allows callers to directly inject trigger scripts, rather than
// fetching them from a remote backend.
const char kBase64TriggerScriptsResponseProtoParameterName[] =
    "TRIGGER_SCRIPTS_BASE64";

TriggerContext::TriggerContext() = default;

TriggerContext::TriggerContext(
    const std::map<std::string, std::string>& parameters,
    const std::string& experiment_ids)
    : parameters_(std::move(parameters)),
      experiment_ids_(std::move(experiment_ids)) {}

TriggerContext::TriggerContext(
    const std::map<std::string, std::string>& parameters,
    const std::string& experiment_ids,
    bool is_cct,
    bool onboarding_shown,
    bool is_direct_action,
    const std::string& caller_account_hash)
    : parameters_(std::move(parameters)),
      experiment_ids_(std::move(experiment_ids)),
      cct_(is_cct),
      onboarding_shown_(onboarding_shown),
      direct_action_(is_direct_action),
      caller_account_hash_(caller_account_hash) {}

TriggerContext::TriggerContext(std::vector<const TriggerContext*> contexts) {
  for (const TriggerContext* context : contexts) {
    for (const auto& parameter : context->GetParameters()) {
      parameters_.insert(parameter);
    }
  }

  for (const TriggerContext* context : contexts) {
    std::string context_experiment_ids = context->GetExperimentIds();
    if (context_experiment_ids.empty())
      continue;

    if (!experiment_ids_.empty())
      experiment_ids_.append(1, ',');

    experiment_ids_.append(context_experiment_ids);
  }

  for (const TriggerContext* context : contexts) {
    cct_ |= context->GetCCT();
    onboarding_shown_ |= context->GetOnboardingShown();
    direct_action_ |= context->GetDirectAction();
    if (caller_account_hash_.empty()) {
      caller_account_hash_ = context->GetCallerAccountHash();
    }
  }
}

TriggerContext::~TriggerContext() = default;

base::Optional<std::string> TriggerContext::GetOverlayColors() const {
  return GetParameter(kOverlayColorParameterName);
}

base::Optional<std::string> TriggerContext::GetPasswordChangeUsername() const {
  return GetParameter(kPasswordChangeUsernameParameterName);
}

base::Optional<std::string>
TriggerContext::GetBase64TriggerScriptsResponseProto() const {
  return GetParameter(kBase64TriggerScriptsResponseProtoParameterName);
}

const std::map<std::string, std::string>& TriggerContext::GetParameters()
    const {
  return parameters_;
}

base::Optional<std::string> TriggerContext::GetParameter(
    const std::string& name) const {
  auto iter = parameters_.find(name);
  if (iter == parameters_.end())
    return base::nullopt;

  return iter->second;
}

std::string TriggerContext::GetExperimentIds() const {
  return experiment_ids_;
}

bool TriggerContext::HasExperimentId(const std::string& experiment_id) const {
  std::vector<std::string> experiments = base::SplitString(
      experiment_ids_, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  return std::find(experiments.begin(), experiments.end(), experiment_id) !=
         experiments.end();
}

bool TriggerContext::GetCCT() const {
  return cct_;
}

bool TriggerContext::GetOnboardingShown() const {
  return onboarding_shown_;
}

bool TriggerContext::GetDirectAction() const {
  return direct_action_;
}

std::string TriggerContext::GetCallerAccountHash() const {
  return caller_account_hash_;
}

}  // namespace autofill_assistant
