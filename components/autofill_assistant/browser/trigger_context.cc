// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_context.h"

#include <string>

#include "base/strings/string_split.h"
#include "components/autofill_assistant/browser/script_parameters.h"

namespace autofill_assistant {

TriggerContext::Options::Options(const std::string& experiment_ids,
                                 bool is_cct,
                                 bool onboarding_shown,
                                 bool is_direct_action,
                                 const std::string& initial_url,
                                 bool is_in_chrome_triggered,
                                 bool is_externally_triggered,
                                 bool skip_autofill_assistant_onboarding,
                                 bool suppress_browsing_features)
    : experiment_ids(experiment_ids),
      is_cct(is_cct),
      onboarding_shown(onboarding_shown),
      is_direct_action(is_direct_action),
      initial_url(initial_url),
      is_in_chrome_triggered(is_in_chrome_triggered),
      is_externally_triggered(is_externally_triggered),
      skip_autofill_assistant_onboarding(skip_autofill_assistant_onboarding),
      suppress_browsing_features(suppress_browsing_features) {}

TriggerContext::Options::Options() = default;
TriggerContext::Options::~Options() = default;

TriggerContext::TriggerContext()
    : script_parameters_(std::make_unique<ScriptParameters>()) {}

TriggerContext::TriggerContext(
    std::unique_ptr<ScriptParameters> script_parameters,
    const Options& options)
    : script_parameters_(std::move(script_parameters)),
      experiment_ids_(options.experiment_ids),
      cct_(options.is_cct),
      onboarding_shown_(options.onboarding_shown),
      direct_action_(options.is_direct_action),
      is_in_chrome_triggered_(options.is_in_chrome_triggered),
      is_externally_triggered_(options.is_externally_triggered),
      skip_autofill_assistant_onboarding_(
          options.skip_autofill_assistant_onboarding),
      suppress_browsing_features_(options.suppress_browsing_features),
      initial_url_(options.initial_url) {}

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
    is_externally_triggered_ |= context->GetIsExternallyTriggered();
    skip_autofill_assistant_onboarding_ |=
        context->GetSkipAutofillAssistantOnboarding();
    suppress_browsing_features_ &= context->GetSuppressBrowsingFeatures();
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

bool TriggerContext::GetIsExternallyTriggered() const {
  return is_externally_triggered_;
}

bool TriggerContext::GetSkipAutofillAssistantOnboarding() const {
  return skip_autofill_assistant_onboarding_ ||
         script_parameters_->GetIsNoRoundtrip().value_or(false);
}

bool TriggerContext::GetSuppressBrowsingFeatures() const {
  return suppress_browsing_features_;
}

}  // namespace autofill_assistant
