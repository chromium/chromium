// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/services/assistant/public/mojom/settings.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class AssistantOptInFlowScreen;

// Interface for dependency injection between AssistantOptInFlowScreen
// and its WebUI representation.
class AssistantOptInFlowScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"assistant-optin-flow"};

  virtual ~AssistantOptInFlowScreenView() = default;

  virtual void Bind(AssistantOptInFlowScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;

 protected:
  AssistantOptInFlowScreenView() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantOptInFlowScreenView);
};

// TODO(updowndota): Refactor to reuse AssistantOptInHandler methods.
class AssistantOptInFlowScreenHandler
    : public BaseScreenHandler,
      public AssistantOptInFlowScreenView,
      public ash::AssistantStateObserver,
      assistant::mojom::SpeakerIdEnrollmentClient {
 public:
  using TView = AssistantOptInFlowScreenView;

  explicit AssistantOptInFlowScreenHandler(
      JSCallsContainer* js_calls_container);
  ~AssistantOptInFlowScreenHandler() override;

  // Set an optional callback that will run when the screen has been
  // initialized.
  void set_on_initialized(base::OnceClosure on_initialized) {
    DCHECK(on_initialized_.is_null());
    on_initialized_ = std::move(on_initialized);
  }

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void RegisterMessages() override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;

  // AssistantOptInFlowScreenView:
  void Bind(AssistantOptInFlowScreen* screen) override;
  void Unbind() override;
  void Show() override;
  void Hide() override;

  // assistant::mojom::SpeakerIdEnrollmentClient:
  void OnListeningHotword() override;
  void OnProcessingHotword() override;
  void OnSpeakerIdEnrollmentDone() override;
  void OnSpeakerIdEnrollmentFailure() override;

  // Setup Assistant settings manager connection.
  void SetupAssistantConnection();

  // Send messages to the page.
  void ShowNextScreen();

  // Handle user opt-in result.
  void OnActivityControlOptInResult(bool opted_in);
  void OnEmailOptInResult(bool opted_in);

  // Called when the UI dialog is closed.
  void OnDialogClosed();

 private:
  // BaseScreenHandler:
  void Initialize() override;

  // ash::AssistantStateObserver:
  void OnAssistantStatusChanged(ash::mojom::AssistantState state) override;

  // Connect to assistant settings manager.
  void BindAssistantSettingsManager();

  // Send GetSettings request for the opt-in UI.
  void SendGetSettingsRequest();

  // Stops the current speaker ID enrollment flow.
  void StopSpeakerIdEnrollment();

  // Send message and consent data to the page.
  void ReloadContent(const base::Value& dict);
  void AddSettingZippy(const std::string& type, const base::Value& data);

  // Handle response from the settings manager.
  void OnGetSettingsResponse(const std::string& settings);
  void OnUpdateSettingsResponse(const std::string& settings);

  // Handler for JS WebUI message.
  void HandleValuePropScreenUserAction(const std::string& action);
  void HandleThirdPartyScreenUserAction(const std::string& action);
  void HandleVoiceMatchScreenUserAction(const std::string& action);
  void HandleGetMoreScreenUserAction(const bool screen_context,
                                     const bool email_opted_in);
  void HandleValuePropScreenShown();
  void HandleThirdPartyScreenShown();
  void HandleVoiceMatchScreenShown();
  void HandleGetMoreScreenShown();
  void HandleLoadingTimeout();
  void HandleFlowFinished();
  void HandleFlowInitialized(const int flow_type);

  AssistantOptInFlowScreen* screen_ = nullptr;

  base::OnceClosure on_initialized_;

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

  // Whether the use has completed voice match enrollment.
  bool voice_match_enrollment_done_ = false;

  // Assistant optin flow type.
  ash::FlowType flow_type_ = ash::FlowType::kConsentFlow;

  // Time that get settings request is sent.
  base::TimeTicks send_request_time_;

  // Counter for the number of loading timeout happens.
  int loading_timeout_counter_ = 0;

  // Whether the screen has been initialized.
  bool initialized_ = false;

  mojo::Receiver<assistant::mojom::SpeakerIdEnrollmentClient> client_receiver_{
      this};
  mojo::Remote<assistant::mojom::AssistantSettingsManager> settings_manager_;
  base::WeakPtrFactory<AssistantOptInFlowScreenHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantOptInFlowScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_
