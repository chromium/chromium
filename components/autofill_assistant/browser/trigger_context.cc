// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_context.h"

#include "base/strings/string_split.h"

namespace autofill_assistant {

TriggerContext::Options::Options(const std::string& _experiment_ids,
                                 bool _is_cct,
                                 bool _onboarding_shown,
                                 bool _is_direct_action,
                                 const std::string& _initial_url,
                                 bool _is_in_chrome_triggered)
    : experiment_ids(_experiment_ids),
      is_cct(_is_cct),
      onboarding_shown(_onboarding_shown),
      is_direct_action(_is_direct_action),
      initial_url(_initial_url),
      is_in_chrome_triggered(_is_in_chrome_triggered) {}

TriggerContext::Options::Options() = default;
TriggerContext::Options::~Options() = default;

TriggerContext::TriggerContext()
    : script_parameters_(std::make_unique<ScriptParameters>()) {}

TriggerContext::TriggerContext(
    std::unique_ptr<ScriptParameters> script_parameters,
    const Options& options)
    : TriggerContext(std::move(script_parameters),
                     options.experiment_ids,
                     options.is_cct,
                     options.onboarding_shown,
                     options.is_direct_action,
                     options.initial_url,
                     options.is_in_chrome_triggered) {}

TriggerContext::TriggerContext(
    std::unique_ptr<ScriptParameters> script_parameters,
    const std::string& experiment_ids,
    bool is_cct,
    bool onboarding_shown,
    bool is_direct_action,
    const std::string& initial_url,
    bool is_in_chrome_triggered)
    : script_parameters_(std::move(script_parameters)),
      experiment_ids_(std::move(experiment_ids)),
      cct_(is_cct),
      onboarding_shown_(onboarding_shown),
      direct_action_(is_direct_action),
      is_in_chrome_triggered_(is_in_chrome_triggered),
      initial_url_(initial_url) {}

TriggerContext::TriggerContext(std::vector<const TriggerContext*> contexts)
    : TriggerContext() {
  for (const TriggerContext* context : contexts) {
    std::string context_experiment_ids = context->GetExperimentIds();
    if (context_experiment_ids.empty())
      continue;

    if (!experiment_ids_.empty())
      experiment_ids_.append(1, ',');

    experiment_ids_.append(context_experiment_ids);
  }

  for (const TriggerContext* context : contexts) {
    script_parameters_->MergeWith(context->GetScriptParameters());
    cct_ |= context->GetCCT();
    onboarding_shown_ |= context->GetOnboardingShown();
    direct_action_ |= context->GetDirectAction();
    is_in_chrome_triggered_ |= context->GetInChromeTriggered();
    if (initial_url_.empty()) {
      initial_url_ = context->GetInitialUrl();
    }
    if (trigger_ui_type_ == TriggerScriptProto::UNSPECIFIED_TRIGGER_UI_TYPE) {
      trigger_ui_type_ = context->GetTriggerUIType();
    }
  }
}

TriggerContext::~TriggerContext() = default;

const ScriptParameters& TriggerContext::GetScriptParameters() const {
  return *script_parameters_;
}

void TriggerContext::SetScriptParameters(
    std::unique_ptr<ScriptParameters> script_parameters) {
  DCHECK(script_parameters);
  script_parameters_ = std::move(script_parameters);
}

std::string TriggerContext::GetExperimentIds() const {
  return experiment_ids_;
}

std::string TriggerContext::GetInitialUrl() const {
  return initial_url_;
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

void TriggerContext::SetOnboardingShown(bool onboarding_shown) {
  onboarding_shown_ = onboarding_shown;
}

bool TriggerContext::GetDirectAction() const {
  return direct_action_;
}

bool TriggerContext::GetInChromeTriggered() const {
  return is_in_chrome_triggered_;
}

TriggerScriptProto::TriggerUIType TriggerContext::GetTriggerUIType() const {
  return trigger_ui_type_;
}

void TriggerContext::SetTriggerUIType(
    TriggerScriptProto::TriggerUIType trigger_ui_type) {
  trigger_ui_type_ = trigger_ui_type;
}

}  // namespace autofill_assistant
