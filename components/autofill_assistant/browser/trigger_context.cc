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

// Parameter that contains the path of the lite script that was used to trigger
// this flow (may be empty).
const char kLiteScriptPathParamaterName[] = "TRIGGER_SCRIPT_USED";

// static
std::unique_ptr<TriggerContext> TriggerContext::CreateEmpty() {
  return std::make_unique<TriggerContextImpl>();
}

// static
std::unique_ptr<TriggerContext> TriggerContext::Create(
    std::map<std::string, std::string> params,
    const std::string& exp) {
  return std::make_unique<TriggerContextImpl>(params, exp);
}

// static
std::unique_ptr<TriggerContext> TriggerContext::Merge(
    std::vector<const TriggerContext*> contexts) {
  return std::make_unique<MergedTriggerContext>(contexts);
}

TriggerContext::TriggerContext() {}
TriggerContext::~TriggerContext() {}

base::Optional<std::string> TriggerContext::GetOverlayColors() const {
  return GetParameter(kOverlayColorParameterName);
}

base::Optional<std::string> TriggerContext::GetPasswordChangeUsername() const {
  return GetParameter(kPasswordChangeUsernameParameterName);
}

bool TriggerContext::WasStartedByTriggerScript() const {
  return GetParameter(kLiteScriptPathParamaterName).has_value();
}

TriggerContextImpl::TriggerContextImpl() {}

TriggerContextImpl::TriggerContextImpl(
    std::map<std::string, std::string> parameters,
    const std::string& experiment_ids)
    : parameters_(std::move(parameters)),
      experiment_ids_(std::move(experiment_ids)) {}

TriggerContextImpl::~TriggerContextImpl() = default;

std::map<std::string, std::string> TriggerContextImpl::GetParameters() const {
  return parameters_;
}

base::Optional<std::string> TriggerContextImpl::GetParameter(
    const std::string& name) const {
  auto iter = parameters_.find(name);
  if (iter == parameters_.end())
    return base::nullopt;

  return iter->second;
}

std::string TriggerContextImpl::experiment_ids() const {
  return experiment_ids_;
}

bool TriggerContextImpl::HasExperimentId(
    const std::string& experiment_id) const {
  std::vector<std::string> experiments = base::SplitString(
      experiment_ids_, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  return std::find(experiments.begin(), experiments.end(), experiment_id) !=
         experiments.end();
}

bool TriggerContextImpl::is_cct() const {
  return cct_;
}

bool TriggerContextImpl::is_onboarding_shown() const {
  return onboarding_shown_;
}

bool TriggerContextImpl::is_direct_action() const {
  return direct_action_;
}

std::string TriggerContextImpl::get_caller_account_hash() const {
  return caller_account_hash_;
}

MergedTriggerContext::MergedTriggerContext(
    std::vector<const TriggerContext*> contexts)
    : contexts_(contexts) {}

MergedTriggerContext::~MergedTriggerContext() {}

std::map<std::string, std::string> MergedTriggerContext::GetParameters() const {
  std::map<std::string, std::string> merged_parameters;
  for (const TriggerContext* context : contexts_) {
    for (const auto& parameter : context->GetParameters()) {
      merged_parameters.insert(parameter);
    }
  }
  return merged_parameters;
}

base::Optional<std::string> MergedTriggerContext::GetParameter(
    const std::string& name) const {
  for (const TriggerContext* context : contexts_) {
    auto opt_value = context->GetParameter(name);
    if (opt_value)
      return opt_value;
  }
  return base::nullopt;
}

std::string MergedTriggerContext::experiment_ids() const {
  std::string experiment_ids;
  for (const TriggerContext* context : contexts_) {
    std::string context_experiment_ids = context->experiment_ids();
    if (context_experiment_ids.empty())
      continue;

    if (!experiment_ids.empty())
      experiment_ids.append(1, ',');

    experiment_ids.append(context->experiment_ids());
  }
  return experiment_ids;
}

bool MergedTriggerContext::HasExperimentId(
    const std::string& experiment_id) const {
  for (const TriggerContext* context : contexts_) {
    if (context->HasExperimentId(experiment_id)) {
      return true;
    }
  }
  return false;
}

bool MergedTriggerContext::is_cct() const {
  for (const TriggerContext* context : contexts_) {
    if (context->is_cct())
      return true;
  }
  return false;
}

bool MergedTriggerContext::is_onboarding_shown() const {
  for (const TriggerContext* context : contexts_) {
    if (context->is_onboarding_shown())
      return true;
  }
  return false;
}

bool MergedTriggerContext::is_direct_action() const {
  for (const TriggerContext* context : contexts_) {
    if (context->is_direct_action())
      return true;
  }
  return false;
}

std::string MergedTriggerContext::get_caller_account_hash() const {
  for (const TriggerContext* context : contexts_) {
    if (!context->get_caller_account_hash().empty())
      return context->get_caller_account_hash();
  }
  return "";
}

}  // namespace autofill_assistant
