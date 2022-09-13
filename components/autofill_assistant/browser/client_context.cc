// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_context.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/version_info/version_info.h"

namespace autofill_assistant {

ClientContextImpl::ClientContextImpl(const Client* client) : client_(client) {
  proto_.mutable_chrome()->set_chrome_version(
      version_info::GetProductNameAndVersionForUserAgent());
  proto_.set_locale(client->GetLocale());
  proto_.set_country(client->GetCountryCode());
// TODO(crbug.com/1321034): Once PlatformDependencies exist and are exposed to
// |Client|, move this check to calls of type |client->IsDesktop()|.
#if BUILDFLAG(IS_ANDROID)
  proto_.set_platform_type(ClientContextProto::PLATFORM_TYPE_ANDROID);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
  proto_.set_platform_type(ClientContextProto::PLATFORM_TYPE_DESKTOP);
#endif

  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  for (const auto& group : active_groups) {
    FieldTrialProto* field_trial =
        proto_.mutable_chrome()->add_active_field_trials();
    field_trial->set_trial_name(group.trial_name);
    field_trial->set_group_name(group.group_name);
  }

  client_->GetDeviceContext().ToProto(proto_.mutable_device_context());
  Update(TriggerContext());
}

void ClientContextImpl::Update(const TriggerContext& trigger_context) {
  proto_.set_accessibility_enabled(client_->IsAccessibilityEnabled());
  const std::string signed_in_email = client_->GetSignedInEmail();
  proto_.set_signed_into_chrome_status(signed_in_email.empty()
                                           ? ClientContextProto::NOT_SIGNED_IN
                                           : ClientContextProto::SIGNED_IN);

  std::string experiment_ids = trigger_context.GetExperimentIds();
  if (!experiment_ids.empty()) {
    proto_.set_experiment_ids(experiment_ids);
  }
  if (trigger_context.GetCCT()) {
    proto_.set_is_cct(true);
  }
  if (trigger_context.GetOnboardingShown()) {
    proto_.set_is_onboarding_shown(true);
  }
  if (trigger_context.GetDirectAction()) {
    proto_.set_is_direct_action(true);
  }
  if (trigger_context.GetInChromeTriggered()) {
    proto_.set_is_in_chrome_triggered(true);
  }

  // TODO(b/156882027): Add an integration test for accounts handling.
  auto caller_email = trigger_context.GetScriptParameters().GetCallerEmail();
  if (!caller_email.has_value()) {
    proto_.set_accounts_matching_status(ClientContextProto::UNKNOWN);
  } else {
    if (signed_in_email == caller_email) {
      proto_.set_accounts_matching_status(
          ClientContextProto::ACCOUNTS_MATCHING);
    } else {
      proto_.set_accounts_matching_status(
          ClientContextProto::ACCOUNTS_NOT_MATCHING);
    }
  }

  auto window_size = client_->GetWindowSize();
  if (window_size.has_value()) {
    proto_.mutable_window_size()->set_width_pixels(window_size.value().first);
    proto_.mutable_window_size()->set_height_pixels(window_size.value().second);
  } else {
    proto_.clear_window_size();
  }

  proto_.set_screen_orientation(client_->GetScreenOrientation());
}

void ClientContextImpl::UpdateAnnotateDomModelContext(int64_t model_version) {
  auto* model_context = proto_.mutable_annotate_dom_model_context();
  model_context->set_model_version(model_version);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAutofillAssistantAnnotateDom)) {
    model_context->set_force_semantic_selection(true);
  }
}

void ClientContextImpl::UpdateJsFlowLibraryLoaded(
    const bool js_flow_library_loaded) {
  proto_.set_js_flow_library_loaded(js_flow_library_loaded);
}

ClientContextProto ClientContextImpl::AsProto() const {
  return proto_;
}

ClientContextProto EmptyClientContext::AsProto() const {
  return ClientContextProto();
}

}  // namespace autofill_assistant
