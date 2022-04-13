// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/headless/external_script_controller_impl.h"

#include "base/time/default_tick_clock.h"
#include "components/autofill_assistant/browser/desktop/starter_delegate_desktop.h"
#include "components/autofill_assistant/browser/starter.h"

namespace autofill_assistant {

ExternalScriptControllerImpl::ExternalScriptControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents_);
  client_ = std::make_unique<ClientHeadless>(web_contents);
}

ExternalScriptControllerImpl::~ExternalScriptControllerImpl() = default;

void ExternalScriptControllerImpl::StartScript(
    const base::flat_map<std::string, std::string>& script_parameters,
    base::OnceCallback<void(ScriptResult)> script_ended_callback) {
  // This ExternalScriptController is currently executing a script, so we return
  // an error.
  if (script_ended_callback_) {
    std::move(script_ended_callback).Run({false});
    return;
  }
  auto* starter = Starter::FromWebContents(web_contents_);
  // The starter has not yet been initialized.
  if (!starter) {
    std::move(script_ended_callback).Run({false});
    return;
  }

  script_ended_callback_ = std::move(script_ended_callback);
  auto parameters = std::make_unique<ScriptParameters>(script_parameters);
  auto trigger_context = std::make_unique<TriggerContext>(
      std::move(parameters),
      /* experiment_ids = */ "",
      // TODO(b/201964911): set the right value for Android flows
      /*is_cct = */ false,
      /*onboarding_shown = */ false,
      /*is_direct_action = */ false,
      /* initial_url = */ "",
      /* is_in_chrome_triggered = */ true);
  starter->CanStart(
      std::move(trigger_context),
      base::BindOnce(&ExternalScriptControllerImpl::OnReadyToStart,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ExternalScriptControllerImpl::OnReadyToStart(
    bool can_start,
    absl::optional<GURL> url,
    std::unique_ptr<TriggerContext> trigger_context) {
  if (!can_start || !url.has_value()) {
    std::move(script_ended_callback_).Run({false});
    return;
  }
  // TODO(b/201964911): At this point we should be sure no other Controller
  // exists on this tab. Add logic to the starter to check that's the case.
  client_->Start(*url, std::move(trigger_context), this);
}

void ExternalScriptControllerImpl::OnShutdown(Metrics::DropOutReason reason) {
  // TODO(b/201964911): since the Controller has not been destroyed yet, this
  // could lead to a race condition if |StartScript| is called again right away
  // after receiving this notification.
  std::move(script_ended_callback_)
      .Run({reason == Metrics::DropOutReason::SCRIPT_SHUTDOWN});
}

void ExternalScriptControllerImpl::OnStateChanged(
    AutofillAssistantState new_state) {}
void ExternalScriptControllerImpl::OnKeyboardSuppressionStateChanged(
    bool should_suppress_keyboard) {}
void ExternalScriptControllerImpl::CloseCustomTab() {}
void ExternalScriptControllerImpl::OnError(const std::string& error_message,
                                           Metrics::DropOutReason reason) {}
void ExternalScriptControllerImpl::OnUserDataChanged(
    const UserData& user_data,
    UserDataFieldChange field_change) {}
void ExternalScriptControllerImpl::OnTouchableAreaChanged(
    const RectF& visual_viewport,
    const std::vector<RectF>& touchable_areas,
    const std::vector<RectF>& restricted_areas) {}
void ExternalScriptControllerImpl::OnViewportModeChanged(ViewportMode mode) {}
void ExternalScriptControllerImpl::OnOverlayColorsChanged(
    const ExecutionDelegate::OverlayColors& colors) {}
void ExternalScriptControllerImpl::OnClientSettingsChanged(
    const ClientSettings& settings) {}
void ExternalScriptControllerImpl::OnShouldShowOverlayChanged(
    bool should_show) {}
void ExternalScriptControllerImpl::OnExecuteScript(
    const std::string& start_message) {}
void ExternalScriptControllerImpl::OnStart(
    const TriggerContext& trigger_context) {}
void ExternalScriptControllerImpl::OnStop() {}
void ExternalScriptControllerImpl::OnResetState() {}
void ExternalScriptControllerImpl::OnUiShownChanged(bool shown) {}
}  // namespace autofill_assistant
