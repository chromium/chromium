// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/assistant/assistant_setup.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_settings.h"
#include "components/sync/protocol/user_consent_types.pb.h"

namespace ash {

// Interface for dependency injection between AssistantOptInFlowScreen
// and its WebUI representation.
class AssistantOptInFlowScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "assistant-optin-flow", "AssistantOptInFlowScreen"};

  AssistantOptInFlowScreenView(const AssistantOptInFlowScreenView&) = delete;
  AssistantOptInFlowScreenView& operator=(const AssistantOptInFlowScreenView&) =
      delete;

  virtual ~AssistantOptInFlowScreenView() = default;

  virtual void Show() = 0;
  virtual base::WeakPtr<AssistantOptInFlowScreenView> AsWeakPtr() = 0;

 protected:
  AssistantOptInFlowScreenView() = default;
};

class AssistantOptInFlowScreenHandler final
    : public BaseScreenHandler,
      public AssistantOptInFlowScreenView,
      public AssistantStateObserver,
      public assistant::SpeakerIdEnrollmentClient {
 public:
  struct ConsentData {
    // Consent token used to complete the opt-in.
    std::string consent_token;

    // An opaque token for audit record.
    std::string ui_audit_key;

    // An enum denoting the Assistant activity control setting type.
    sync_pb::UserConsentTypes::AssistantActivityControlConsent::SettingType
        setting_type;
  };

  using TView = AssistantOptInFlowScreenView;

  explicit AssistantOptInFlowScreenHandler(bool is_oobe = false);

  AssistantOptInFlowScreenHandler(const AssistantOptInFlowScreenHandler&) =
      delete;
  AssistantOptInFlowScreenHandler& operator=(
      const AssistantOptInFlowScreenHandler&) = delete;

  ~AssistantOptInFlowScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void DeclareJSCallbacks() override;
  void GetAdditionalParameters(base::Value::Dict* dict) override;

  // AssistantOptInFlowScreenView:
  void Show() override;
  base::WeakPtr<AssistantOptInFlowScreenView> AsWeakPtr() override;

  // assistant::SpeakerIdEnrollmentClient:
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
  void OnScreenContextOptInResult(bool opted_in);

  // Called when the UI dialog is closed.
  void OnDialogClosed();

 private:
  // AssistantStateObserver:
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantStatusChanged(assistant::AssistantStatus status) override;

  // Send GetSettings request for the opt-in UI.
  void SendGetSettingsRequest();

  // Stops the current speaker ID enrollment flow.
  void StopSpeakerIdEnrollment();

  // Send message and consent data to the page.
  void ReloadContent(base::Value::Dict dict);
  void AddSettingZippy(const std::string& type, base::Value::List data);

  // Update value prop screen to show the next settings.
  void UpdateValuePropScreen();

  // Handle response from the settings manager.
  void OnGetSettingsResponse(const std::string& settings);
  void OnUpdateSettingsResponse(const std::string& settings);

  // Handler for JS WebUI message.
  void HandleValuePropScreenUserAction(const std::string& action);
  void HandleRelatedInfoScreenUserAction(const std::string& action);
  void HandleVoiceMatchScreenUserAction(const std::string& action);
  void HandleValuePropScreenShown();
  void HandleRelatedInfoScreenShown();
  void HandleVoiceMatchScreenShown();
  void HandleLoadingTimeout();
  void HandleFlowFinished();
  void HandleFlowInitialized(const int flow_type);

  // Power related
  bool DeviceHasBattery();

  // Whether activity control is needed for user.
  bool activity_control_needed_ = true;

  // Whether the user has started voice match enrollment.
  bool voice_match_enrollment_started_ = false;

  // Whether the use has completed voice match enrollment.
  bool voice_match_enrollment_done_ = false;

  // Whether error occurs during voice match enrollment.
  bool voice_match_enrollment_error_ = false;

  // Assistant optin flow type.
  FlowType flow_type_ = FlowType::kConsentFlow;

  // Time that get settings request is sent.
  base::TimeTicks send_request_time_;

  // Counter for the number of loading timeout happens.
  int loading_timeout_counter_ = 0;

  // Whether the screen has been initialized.
  bool initialized_ = false;

  // Whether the user has opted in/out any activity control consent.
  bool has_opted_out_any_consent_ = false;
  bool has_opted_in_any_consent_ = false;

  // Whether assistant shown during OOBE or in session.
  bool is_oobe_ = false;

  // Used to record related information of activity control consents which are
  // pending for user action.
  base::circular_deque<ConsentData> pending_consent_data_;

  base::WeakPtrFactory<AssistantOptInFlowScreenHandler> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ASSISTANT_OPTIN_FLOW_SCREEN_HANDLER_H_
