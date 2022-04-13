// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_EXTERNAL_SCRIPT_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_EXTERNAL_SCRIPT_CONTROLLER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "components/autofill_assistant/browser/autofill_assistant_impl.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/controller_observer.h"
#include "components/autofill_assistant/browser/execution_delegate.h"
#include "components/autofill_assistant/browser/headless/client_headless.h"
#include "components/autofill_assistant/browser/script_executor_ui_delegate.h"

namespace autofill_assistant {

class ExternalScriptControllerImpl : public ExternalScriptController,
                                     public ControllerObserver {
 public:
  ExternalScriptControllerImpl(content::WebContents* web_contents);

  ExternalScriptControllerImpl(const ExternalScriptControllerImpl&) = delete;
  ExternalScriptControllerImpl& operator=(const ExternalScriptControllerImpl&) =
      delete;

  ~ExternalScriptControllerImpl() override;

  // Overrides ExternalScriptController.
  void StartScript(
      const base::flat_map<std::string, std::string>& script_parameters,
      base::OnceCallback<void(ScriptResult)> script_ended_callback) override;

  // Overrides ControllerObserver.
  // TODO(b/201964911): Add empty default ControllerObserver to avoid having all
  // of these no-op implementations here.
  void OnStateChanged(AutofillAssistantState new_state) override;
  void OnKeyboardSuppressionStateChanged(
      bool should_suppress_keyboard) override;
  void CloseCustomTab() override;
  void OnError(const std::string& error_message,
               Metrics::DropOutReason reason) override;
  void OnUserDataChanged(const UserData& user_data,
                         UserDataFieldChange field_change) override;
  void OnTouchableAreaChanged(
      const RectF& visual_viewport,
      const std::vector<RectF>& touchable_areas,
      const std::vector<RectF>& restricted_areas) override;
  void OnViewportModeChanged(ViewportMode mode) override;
  void OnOverlayColorsChanged(
      const ExecutionDelegate::OverlayColors& colors) override;
  void OnClientSettingsChanged(const ClientSettings& settings) override;
  void OnShouldShowOverlayChanged(bool should_show) override;
  void OnExecuteScript(const std::string& start_message) override;
  void OnStart(const TriggerContext& trigger_context) override;
  void OnStop() override;
  void OnResetState() override;
  void OnUiShownChanged(bool shown) override;
  void OnShutdown(Metrics::DropOutReason reason) override;

 private:
  void OnReadyToStart(bool can_start,
                      absl::optional<GURL> url,
                      std::unique_ptr<TriggerContext> trigger_context);
  content::WebContents* web_contents_;
  std::unique_ptr<ClientHeadless> client_;

  base::OnceCallback<void(ScriptResult)> script_ended_callback_;

  base::WeakPtrFactory<ExternalScriptControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_EXTERNAL_SCRIPT_CONTROLLER_IMPL_H_
