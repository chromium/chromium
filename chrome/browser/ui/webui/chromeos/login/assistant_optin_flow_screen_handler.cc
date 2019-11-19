// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/assistant_optin_flow_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/assistant/assistant_service_connection.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

namespace {

constexpr char kSkipPressed[] = "skip-pressed";
constexpr char kNextPressed[] = "next-pressed";
constexpr char kRecordPressed[] = "record-pressed";
constexpr char kFlowFinished[] = "flow-finished";
constexpr char kReloadRequested[] = "reload-requested";
constexpr char kVoiceMatchDone[] = "voice-match-done";

bool IsKnownEnumValue(ash::FlowType flow_type) {
  return flow_type == ash::FlowType::kConsentFlow ||
         flow_type == ash::FlowType::kSpeakerIdEnrollment ||
         flow_type == ash::FlowType::kSpeakerIdRetrain;
}

}  // namespace

constexpr StaticOobeScreenId AssistantOptInFlowScreenView::kScreenId;

AssistantOptInFlowScreenHandler::AssistantOptInFlowScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.AssistantOptInFlowScreen.userActed");
}

AssistantOptInFlowScreenHandler::~AssistantOptInFlowScreenHandler() {
  if (client_receiver_.is_bound())
    StopSpeakerIdEnrollment();
  if (ash::AssistantState::Get())
    ash::AssistantState::Get()->RemoveObserver(this);
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void AssistantOptInFlowScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("locale", g_browser_process->GetApplicationLocale());
  builder->Add("assistantLogo", IDS_ASSISTANT_LOGO);
  builder->Add("assistantOptinLoading", IDS_ASSISTANT_VALUE_PROP_LOADING);
  builder->Add("assistantOptinLoadErrorTitle",
               IDS_ASSISTANT_VALUE_PROP_LOAD_ERROR_TITLE);
  builder->Add("assistantOptinLoadErrorMessage",
               IDS_ASSISTANT_VALUE_PROP_LOAD_ERROR_MESSAGE);
  builder->Add("assistantOptinSkipButton",
               IDS_ASSISTANT_VALUE_PROP_SKIP_BUTTON);
  builder->Add("assistantOptinRetryButton",
               IDS_ASSISTANT_VALUE_PROP_RETRY_BUTTON);
  builder->Add("assistantUserImage", IDS_ASSISTANT_OOBE_USER_IMAGE);
  builder->Add("assistantVoiceMatchTitle", IDS_ASSISTANT_VOICE_MATCH_TITLE);
  builder->Add("assistantVoiceMatchMessage", IDS_ASSISTANT_VOICE_MATCH_MESSAGE);
  builder->Add("assistantVoiceMatchNoDspMessage",
               IDS_ASSISTANT_VOICE_MATCH_NO_DSP_MESSAGE);
  builder->Add("assistantVoiceMatchRecording",
               IDS_ASSISTANT_VOICE_MATCH_RECORDING);
  builder->Add("assistantVoiceMatchCompleted",
               IDS_ASSISTANT_VOICE_MATCH_COMPLETED);
  builder->Add("assistantVoiceMatchFooter", IDS_ASSISTANT_VOICE_MATCH_FOOTER);
  builder->Add("assistantVoiceMatchInstruction0",
               IDS_ASSISTANT_VOICE_MATCH_INSTRUCTION0);
  builder->Add("assistantVoiceMatchInstruction1",
               IDS_ASSISTANT_VOICE_MATCH_INSTRUCTION1);
  builder->Add("assistantVoiceMatchInstruction2",
               IDS_ASSISTANT_VOICE_MATCH_INSTRUCTION2);
  builder->Add("assistantVoiceMatchInstruction3",
               IDS_ASSISTANT_VOICE_MATCH_INSTRUCTION3);
  builder->Add("assistantVoiceMatchComplete",
               IDS_ASSISTANT_VOICE_MATCH_COMPLETE);
  builder->Add("assistantVoiceMatchUploading",
               IDS_ASSISTANT_VOICE_MATCH_UPLOADING);
  builder->Add("assistantVoiceMatchA11yMessage",
               IDS_ASSISTANT_VOICE_MATCH_ACCESSIBILITY_MESSAGE);
  builder->Add("assistantVoiceMatchAlreadySetupTitle",
               IDS_ASSISTANT_VOICE_MATCH_ALREADY_SETUP_TITLE);
  builder->Add("assistantVoiceMatchAlreadySetupMessage",
               IDS_ASSISTANT_VOICE_MATCH_ALREADY_SETUP_MESSAGE);
  builder->Add("assistantOptinOKButton", IDS_OOBE_OK_BUTTON_TEXT);
  builder->Add("assistantOptinNoThanksButton", IDS_ASSISTANT_NO_THANKS_BUTTON);
  builder->Add("assistantOptinLaterButton", IDS_ASSISTANT_LATER_BUTTON);
  builder->Add("assistantOptinAgreeButton", IDS_ASSISTANT_AGREE_BUTTON);
  builder->Add("assistantOptinSaveButton", IDS_ASSISTANT_SAVE_BUTTON);
  builder->Add("assistantOptinWaitMessage", IDS_ASSISTANT_WAIT_MESSAGE);
  builder->Add("assistantReadyTitle", IDS_ASSISTANT_READY_SCREEN_TITLE);
  builder->AddF("assistantReadyMessage", IDS_ASSISTANT_READY_SCREEN_MESSAGE,
                ui::GetChromeOSDeviceName());
  builder->Add("assistantReadyButton", IDS_ASSISTANT_DONE_BUTTON);
  builder->Add("back", IDS_EULA_BACK_BUTTON);
  builder->Add("next", IDS_EULA_NEXT_BUTTON);
  builder->Add("assistantOobePopupOverlayLoading",
               IDS_ASSISTANT_OOBE_POPUP_OVERLAY_LOADING);
}

void AssistantOptInFlowScreenHandler::RegisterMessages() {
  AddCallback(
      "login.AssistantOptInFlowScreen.ValuePropScreen.userActed",
      &AssistantOptInFlowScreenHandler::HandleValuePropScreenUserAction);
  AddCallback(
      "login.AssistantOptInFlowScreen.ThirdPartyScreen.userActed",
      &AssistantOptInFlowScreenHandler::HandleThirdPartyScreenUserAction);
  AddCallback(
      "login.AssistantOptInFlowScreen.VoiceMatchScreen.userActed",
      &AssistantOptInFlowScreenHandler::HandleVoiceMatchScreenUserAction);
  AddCallback("login.AssistantOptInFlowScreen.GetMoreScreen.userActed",
              &AssistantOptInFlowScreenHandler::HandleGetMoreScreenUserAction);
  AddCallback("login.AssistantOptInFlowScreen.ValuePropScreen.screenShown",
              &AssistantOptInFlowScreenHandler::HandleValuePropScreenShown);
  AddCallback("login.AssistantOptInFlowScreen.ThirdPartyScreen.screenShown",
              &AssistantOptInFlowScreenHandler::HandleThirdPartyScreenShown);
  AddCallback("login.AssistantOptInFlowScreen.VoiceMatchScreen.screenShown",
              &AssistantOptInFlowScreenHandler::HandleVoiceMatchScreenShown);
  AddCallback("login.AssistantOptInFlowScreen.GetMoreScreen.screenShown",
              &AssistantOptInFlowScreenHandler::HandleGetMoreScreenShown);
  AddCallback("login.AssistantOptInFlowScreen.LoadingScreen.timeout",
              &AssistantOptInFlowScreenHandler::HandleLoadingTimeout);
  AddCallback("login.AssistantOptInFlowScreen.flowFinished",
              &AssistantOptInFlowScreenHandler::HandleFlowFinished);
  AddCallback("login.AssistantOptInFlowScreen.initialized",
              &AssistantOptInFlowScreenHandler::HandleFlowInitialized);
}

void AssistantOptInFlowScreenHandler::GetAdditionalParameters(
    base::DictionaryValue* dict) {
  dict->SetBoolean("hotwordDspAvailable", chromeos::IsHotwordDspAvailable());
  dict->SetBoolean("voiceMatchDisabled",
                   chromeos::assistant::features::IsVoiceMatchDisabled());
  BaseScreenHandler::GetAdditionalParameters(dict);
}

void AssistantOptInFlowScreenHandler::Bind(AssistantOptInFlowScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
  screen_ = screen;
  if (page_is_ready())
    Initialize();
}

void AssistantOptInFlowScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void AssistantOptInFlowScreenHandler::Show() {
  if (!page_is_ready() || !screen_) {
    show_on_init_ = true;
    return;
  }

  SetupAssistantConnection();

  ShowScreen(kScreenId);
}

void AssistantOptInFlowScreenHandler::Hide() {}

void AssistantOptInFlowScreenHandler::Initialize() {
  if (!screen_ || !show_on_init_)
    return;

  Show();
  show_on_init_ = false;
}

void AssistantOptInFlowScreenHandler::OnListeningHotword() {
  CallJS("login.AssistantOptInFlowScreen.onVoiceMatchUpdate",
         base::Value("listen"));
}

void AssistantOptInFlowScreenHandler::OnProcessingHotword() {
  CallJS("login.AssistantOptInFlowScreen.onVoiceMatchUpdate",
         base::Value("process"));
}

void AssistantOptInFlowScreenHandler::OnSpeakerIdEnrollmentDone() {
  StopSpeakerIdEnrollment();
  CallJS("login.AssistantOptInFlowScreen.onVoiceMatchUpdate",
         base::Value("done"));
}

void AssistantOptInFlowScreenHandler::OnSpeakerIdEnrollmentFailure() {
  StopSpeakerIdEnrollment();
  RecordAssistantOptInStatus(VOICE_MATCH_ENROLLMENT_ERROR);
  CallJS("login.AssistantOptInFlowScreen.onVoiceMatchUpdate",
         base::Value("failure"));
  LOG(ERROR) << "Speaker ID enrollment failure.";
}

void AssistantOptInFlowScreenHandler::SetupAssistantConnection() {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();

  // If Assistant is disabled by domain admin, end the flow.
  if (prefs->GetBoolean(assistant::prefs::kAssistantDisabledByPolicy)) {
    HandleFlowFinished();
    return;
  }

  // Make sure enable Assistant service since we need it during the flow.
  prefs->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, true);

  if (ash::AssistantState::Get()->assistant_state() ==
      ash::mojom::AssistantState::NOT_READY) {
    ash::AssistantState::Get()->AddObserver(this);
  } else {
    BindAssistantSettingsManager();
  }
}

void AssistantOptInFlowScreenHandler::ShowNextScreen() {
  CallJS("login.AssistantOptInFlowScreen.showNextScreen");
}

void AssistantOptInFlowScreenHandler::OnActivityControlOptInResult(
    bool opted_in) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  RecordActivityControlConsent(profile, ui_audit_key_, opted_in);
  if (opted_in) {
    RecordAssistantOptInStatus(ACTIVITY_CONTROL_ACCEPTED);
    settings_manager_->UpdateSettings(
        GetSettingsUiUpdate(consent_token_).SerializeAsString(),
        base::BindOnce(
            &AssistantOptInFlowScreenHandler::OnUpdateSettingsResponse,
            weak_factory_.GetWeakPtr()));
  } else {
    RecordAssistantOptInStatus(ACTIVITY_CONTROL_SKIPPED);
    profile->GetPrefs()->SetInteger(assistant::prefs::kAssistantConsentStatus,
                                    assistant::prefs::ConsentStatus::kUnknown);
    HandleFlowFinished();
  }
}

void AssistantOptInFlowScreenHandler::OnEmailOptInResult(bool opted_in) {
  if (!email_optin_needed_) {
    DCHECK(!opted_in);
    HandleFlowFinished();
    return;
  }

  RecordAssistantOptInStatus(opted_in ? EMAIL_OPTED_IN : EMAIL_OPTED_OUT);
  settings_manager_->UpdateSettings(
      GetEmailOptInUpdate(opted_in).SerializeAsString(),
      base::BindOnce(&AssistantOptInFlowScreenHandler::OnUpdateSettingsResponse,
                     weak_factory_.GetWeakPtr()));
}

void AssistantOptInFlowScreenHandler::OnDialogClosed() {
  // Disable hotword for user if voice match enrollment has not completed.
  if (!voice_match_enrollment_done_ &&
      flow_type_ == ash::FlowType::kSpeakerIdEnrollment) {
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
        assistant::prefs::kAssistantHotwordEnabled, false);
  }
}

void AssistantOptInFlowScreenHandler::OnAssistantStatusChanged(
    ash::mojom::AssistantState state) {
  if (state != ash::mojom::AssistantState::NOT_READY) {
    BindAssistantSettingsManager();
    ash::AssistantState::Get()->RemoveObserver(this);
  }
}

void AssistantOptInFlowScreenHandler::BindAssistantSettingsManager() {
  if (settings_manager_.is_bound())
    return;

  // Set up settings mojom.
  AssistantServiceConnection::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->service()
      ->BindSettingsManager(settings_manager_.BindNewPipeAndPassReceiver());

  if (initialized_) {
    SendGetSettingsRequest();
  }
}

void AssistantOptInFlowScreenHandler::SendGetSettingsRequest() {
  assistant::SettingsUiSelector selector = GetSettingsUiSelector();
  settings_manager_->GetSettings(
      selector.SerializeAsString(),
      base::BindOnce(&AssistantOptInFlowScreenHandler::OnGetSettingsResponse,
                     weak_factory_.GetWeakPtr()));
  send_request_time_ = base::TimeTicks::Now();
}

void AssistantOptInFlowScreenHandler::StopSpeakerIdEnrollment() {
  settings_manager_->StopSpeakerIdEnrollment(base::DoNothing());
  // Reset the receiver so it can be used again if enrollment is retried.
  client_receiver_.reset();
}

void AssistantOptInFlowScreenHandler::ReloadContent(const base::Value& dict) {
  CallJS("login.AssistantOptInFlowScreen.reloadContent", dict);
}

void AssistantOptInFlowScreenHandler::AddSettingZippy(const std::string& type,
                                                      const base::Value& data) {
  CallJS("login.AssistantOptInFlowScreen.addSettingZippy", type, data);
}

void AssistantOptInFlowScreenHandler::OnGetSettingsResponse(
    const std::string& settings) {
  const base::TimeDelta time_since_request_sent =
      base::TimeTicks::Now() - send_request_time_;
  UMA_HISTOGRAM_TIMES("Assistant.OptInFlow.GetSettingsRequestTime",
                      time_since_request_sent);

  assistant::SettingsUi settings_ui;
  if (!settings_ui.ParseFromString(settings)) {
    LOG(ERROR) << "Failed to parse get settings response.";
    HandleFlowFinished();
    return;
  }

  if (settings_ui.has_gaia_user_context_ui()) {
    auto gaia_user_context_ui = settings_ui.gaia_user_context_ui();
    if (gaia_user_context_ui.assistant_disabled_by_dasher_domain()) {
      DVLOG(1) << "Assistant is disabled by domain policy. Skip Assistant "
                  "opt-in flow.";
      PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
      prefs->SetBoolean(assistant::prefs::kAssistantDisabledByPolicy, true);
      prefs->SetBoolean(chromeos::assistant::prefs::kAssistantEnabled, false);
      HandleFlowFinished();
      return;
    }

    if (gaia_user_context_ui.waa_disabled_by_dasher_domain()) {
      DVLOG(1) << "Web & app activity is disabled by domain policy. Skip "
                  "Assistant opt-in flow.";
      HandleFlowFinished();
      return;
    }
  }

  DCHECK(settings_ui.has_consent_flow_ui());

  RecordAssistantOptInStatus(FLOW_STARTED);
  auto consent_ui = settings_ui.consent_flow_ui().consent_ui();
  auto activity_control_ui = consent_ui.activity_control_ui();
  auto third_party_disclosure_ui = consent_ui.third_party_disclosure_ui();

  consent_token_ = activity_control_ui.consent_token();
  ui_audit_key_ = activity_control_ui.ui_audit_key();

  // Process activity control data.
  bool skip_activity_control = !activity_control_ui.setting_zippy().size();
  if (skip_activity_control) {
    // No need to consent. Move to the next screen.
    activity_control_needed_ = false;
    PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();

    bool consented =
        settings_ui.consent_flow_ui().consent_status() ==
            assistant::ConsentFlowUi_ConsentStatus_ALREADY_CONSENTED ||
        settings_ui.consent_flow_ui().consent_status() ==
            assistant::ConsentFlowUi_ConsentStatus_ASK_FOR_CONSENT;

    prefs->SetInteger(
        assistant::prefs::kAssistantConsentStatus,
        consented ? assistant::prefs::ConsentStatus::kActivityControlAccepted
                  : assistant::prefs::ConsentStatus::kUnknown);
  } else {
    AddSettingZippy("settings",
                    CreateZippyData(activity_control_ui.setting_zippy()));
  }

  // Process third party disclosure data.
  bool skip_third_party_disclosure =
      skip_activity_control && !third_party_disclosure_ui.disclosures().size();
  if (third_party_disclosure_ui.disclosures().size()) {
    AddSettingZippy("disclosure", CreateDisclosureData(
                                      third_party_disclosure_ui.disclosures()));
  } else if (!skip_third_party_disclosure) {
    // TODO(llin): Show an error message and log it properly.
    LOG(ERROR) << "Missing third Party disclosure data.";
    return;
  }

  // Process get more data.
  email_optin_needed_ = settings_ui.has_email_opt_in_ui() &&
                        settings_ui.email_opt_in_ui().has_title();
  auto* profile_helper = ProfileHelper::Get();
  const auto* user = user_manager::UserManager::Get()->GetActiveUser();
  auto get_more_data =
      CreateGetMoreData(email_optin_needed_, settings_ui.email_opt_in_ui(),
                        profile_helper->GetProfileByUser(user)->GetPrefs());

  bool skip_get_more =
      skip_third_party_disclosure && !get_more_data.GetList().size();
  if (get_more_data.GetList().size()) {
    AddSettingZippy("get-more", get_more_data);
  } else if (!skip_get_more) {
    // TODO(llin): Show an error message and log it properly.
    LOG(ERROR) << "Missing get more data.";
    return;
  }

  // Pass string constants dictionary.
  auto dictionary = GetSettingsUiStrings(settings_ui, activity_control_needed_);
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  dictionary.SetKey("voiceMatchEnforcedOff",
                    base::Value(IsVoiceMatchEnforcedOff(prefs)));
  ReloadContent(dictionary);

  // Now that screen's content has been reloaded, skip screens that can be
  // skipped - if this is done before content reload, internal screen
  // transitions might be based on incorrect data. For example, if both activity
  // control and third party disclosure are skipped, opt in flow might skip
  // voice match enrollment, thinking that voice match is not enabled.

  // Skip activity control and users will be in opted out mode.
  if (skip_activity_control)
    ShowNextScreen();

  if (skip_third_party_disclosure)
    ShowNextScreen();

  // If voice match is enabled, the screen that follows third party disclosure
  // is the "voice match" screen, not "get more" screen.
  if (skip_get_more && IsVoiceMatchEnforcedOff(prefs))
    ShowNextScreen();
}

void AssistantOptInFlowScreenHandler::OnUpdateSettingsResponse(
    const std::string& result) {
  assistant::SettingsUiUpdateResult ui_result;
  ui_result.ParseFromString(result);

  if (ui_result.has_consent_flow_update_result()) {
    if (ui_result.consent_flow_update_result().update_status() !=
        assistant::ConsentFlowUiUpdateResult::SUCCESS) {
      // TODO(updowndta): Handle consent update failure.
      LOG(ERROR) << "Consent udpate error.";
    } else if (activity_control_needed_) {
      activity_control_needed_ = false;
      PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
      prefs->SetInteger(
          assistant::prefs::kAssistantConsentStatus,
          assistant::prefs::ConsentStatus::kActivityControlAccepted);
    }
  }

  if (ui_result.has_email_opt_in_update_result()) {
    if (ui_result.email_opt_in_update_result().update_status() !=
        assistant::EmailOptInUpdateResult::SUCCESS) {
      // TODO(updowndta): Handle email optin update failure.
      LOG(ERROR) << "Email OptIn udpate error.";
    }
    HandleFlowFinished();
    return;
  }

  ShowNextScreen();
}

void AssistantOptInFlowScreenHandler::HandleValuePropScreenUserAction(
    const std::string& action) {
  if (action == kSkipPressed) {
    OnActivityControlOptInResult(false);
  } else if (action == kNextPressed) {
    OnActivityControlOptInResult(true);
  } else if (action == kReloadRequested) {
    if (settings_manager_.is_bound()) {
      SendGetSettingsRequest();
    } else {
      LOG(ERROR) << "Settings mojom failed to setup. Check Assistant service.";
    }
  }
}

void AssistantOptInFlowScreenHandler::HandleThirdPartyScreenUserAction(
    const std::string& action) {
  if (action == kNextPressed) {
    RecordAssistantOptInStatus(THIRD_PARTY_CONTINUED);
    ShowNextScreen();
  }
}

void AssistantOptInFlowScreenHandler::HandleVoiceMatchScreenUserAction(
    const std::string& action) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();

  if (action == kVoiceMatchDone) {
    RecordAssistantOptInStatus(VOICE_MATCH_ENROLLMENT_DONE);
    voice_match_enrollment_done_ = true;
    ShowNextScreen();
  } else if (action == kSkipPressed) {
    RecordAssistantOptInStatus(VOICE_MATCH_ENROLLMENT_SKIPPED);
    if (flow_type_ != ash::FlowType::kSpeakerIdRetrain) {
      // No need to disable hotword for retrain flow since user has a model.
      prefs->SetBoolean(assistant::prefs::kAssistantHotwordEnabled, false);
    }
    StopSpeakerIdEnrollment();
    ShowNextScreen();
  } else if (action == kRecordPressed) {
    if (!prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled)) {
      prefs->SetBoolean(assistant::prefs::kAssistantHotwordEnabled, true);
    }

    settings_manager_->StartSpeakerIdEnrollment(
        flow_type_ == ash::FlowType::kSpeakerIdRetrain,
        client_receiver_.BindNewPipeAndPassRemote());
  }
}

void AssistantOptInFlowScreenHandler::HandleGetMoreScreenUserAction(
    const bool screen_context,
    const bool email_opted_in) {
  RecordAssistantOptInStatus(GET_MORE_CONTINUED);
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(assistant::prefs::kAssistantContextEnabled, screen_context);
  OnEmailOptInResult(email_opted_in);
}

void AssistantOptInFlowScreenHandler::HandleValuePropScreenShown() {
  RecordAssistantOptInStatus(ACTIVITY_CONTROL_SHOWN);
}

void AssistantOptInFlowScreenHandler::HandleThirdPartyScreenShown() {
  RecordAssistantOptInStatus(THIRD_PARTY_SHOWN);
}

void AssistantOptInFlowScreenHandler::HandleVoiceMatchScreenShown() {
  RecordAssistantOptInStatus(VOICE_MATCH_SHOWN);
}

void AssistantOptInFlowScreenHandler::HandleGetMoreScreenShown() {
  RecordAssistantOptInStatus(GET_MORE_SHOWN);
}

void AssistantOptInFlowScreenHandler::HandleLoadingTimeout() {
  ++loading_timeout_counter_;
}

void AssistantOptInFlowScreenHandler::HandleFlowFinished() {
  auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (!prefs->GetUserPrefValue(assistant::prefs::kAssistantConsentStatus)) {
    // Set consent status to unknown if user consent is needed but not provided.
    prefs->SetInteger(
        assistant::prefs::kAssistantConsentStatus,
        activity_control_needed_
            ? assistant::prefs::ConsentStatus::kUnknown
            : assistant::prefs::ConsentStatus::kActivityControlAccepted);
  }

  UMA_HISTOGRAM_EXACT_LINEAR("Assistant.OptInFlow.LoadingTimeoutCount",
                             loading_timeout_counter_, 10);
  if (screen_)
    screen_->OnUserAction(kFlowFinished);
  else
    CallJS("login.AssistantOptInFlowScreen.closeDialog");
}

void AssistantOptInFlowScreenHandler::HandleFlowInitialized(
    const int flow_type) {
  auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (!prefs->GetBoolean(chromeos::assistant::prefs::kAssistantEnabled)) {
    HandleFlowFinished();
    return;
  }

  initialized_ = true;

  if (on_initialized_)
    std::move(on_initialized_).Run();

  DCHECK(IsKnownEnumValue(static_cast<ash::FlowType>(flow_type)));
  flow_type_ = static_cast<ash::FlowType>(flow_type);

  if (settings_manager_.is_bound() &&
      flow_type_ == ash::FlowType::kConsentFlow) {
    SendGetSettingsRequest();
  }
}

}  // namespace chromeos
