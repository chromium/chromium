// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/voice_interaction/voice_interaction_controller_client.h"
#include "chrome/browser/chromeos/login/screens/assistant_optin_flow_screen_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/services/assistant/public/mojom/settings.mojom.h"

namespace chromeos {

// TODO(updowndota): Refactor to reuse AssistantOptInHandler methods.
class AssistantOptInFlowScreenHandler
    : public BaseScreenHandler,
      public AssistantOptInFlowScreenView,
      public arc::VoiceInteractionControllerClient::Observer {
 public:
  AssistantOptInFlowScreenHandler();
  ~AssistantOptInFlowScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void RegisterMessages() override;

  // AssistantOptInFlowScreenView:
  void Bind(AssistantOptInFlowScreen* screen) override;
  void Unbind() override;
  void Show() override;
  void Hide() override;

  // Setup Assistant settings manager connection.
  void SetupAssistantConnection();

  // Send messages to the page.
  void ShowNextScreen();

  // Handle user opt-in result.
  void OnActivityControlOptInResult(bool opted_in);
  void OnEmailOptInResult(bool opted_in);

 private:
  // BaseScreenHandler:
  void Initialize() override;

  // arc::VoiceInteractionControllerClient::Observer overrides
  void OnStateChanged(ash::mojom::VoiceInteractionState state) override;

  // Connect to assistant settings manager.
  void BindAssistantSettingsManager();

  // Send GetSettings request for the opt-in UI.
  void SendGetSettingsRequest();

  // Send message and consent data to the page.
  void ReloadContent(const base::Value& dict);
  void AddSettingZippy(const std::string& type, const base::Value& data);

  // Handle response from the settings manager.
  void OnGetSettingsResponse(const std::string& settings);
  void OnUpdateSettingsResponse(const std::string& settings);

  // Handler for JS WebUI message.
  void HandleValuePropScreenUserAction(const std::string& action);
  void HandleThirdPartyScreenUserAction(const std::string& action);
  void HandleGetMoreScreenUserAction(const bool screen_context,
                                     const bool email_opted_in);
  void HandleReadyScreenUserAction(const std::string& action);
  void HandleValuePropScreenShown();
  void HandleThirdPartyScreenShown();
  void HandleGetMoreScreenShown();
  void HandleReadyScreenShown();
  void HandleLoadingTimeout();
  void HandleHotwordResult(bool enable_hotword);
  void HandleFlowFinished();
  void HandleFlowInitialized();

  AssistantOptInFlowScreen* screen_ = nullptr;

  // Whether the screen should be shown right after initialization.
  bool show_on_init_ = false;

  // Consent token used to complete the opt-in.
  std::string consent_token_;

  // An opaque token for audit record.
  std::string ui_audit_key_;

  // Whether activity control is needed for user.
  bool activity_control_needed_ = true;

  // Whether email optin is needed for user.
  bool email_optin_needed_ = false;

  // Whether user chose to enable hotword.
  bool enable_hotword_ = true;

  // Time that get settings request is sent.
  base::TimeTicks send_request_time_;

  // Counter for the number of loading timeout happens.
  int loading_timeout_counter_ = 0;

  assistant::mojom::AssistantSettingsManagerPtr settings_manager_;
  base::WeakPtrFactory<AssistantOptInFlowScreenHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantOptInFlowScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_
