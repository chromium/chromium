// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/assistant_optin_flow_screen_handler.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/assistant_optin_flow_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/assistant_optin/assistant_optin_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/public/proto/get_settings_ui.pb.h"
#include "chromeos/ash/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

constexpr char kSkipPressed[] = "skip-pressed";
constexpr char kNextPressed[] = "next-pressed";
constexpr char kRecordPressed[] = "record-pressed";
constexpr char kFlowFinished[] = "flow-finished";
constexpr char kReloadRequested[] = "reload-requested";
constexpr char kVoiceMatchDone[] = "voice-match-done";

bool IsKnownEnumValue(FlowType flow_type) {
  return flow_type == FlowType::kConsentFlow ||
         flow_type == FlowType::kSpeakerIdEnrollment ||
         flow_type == FlowType::kSpeakerIdRetrain;
}

// Returns given name of the user if a child account is in use; returns empty
// string if user is not a child.
std::u16string GetGivenNameIfIsChild() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!user || !user->IsChild())
    return std::u16string();
  return user->GetGivenName();
}

}  // namespace

AssistantOptInFlowScreenHandler::AssistantOptInFlowScreenHandler(bool is_oobe)
    : BaseScreenHandler(kScreenId), is_oobe_(is_oobe) {}

AssistantOptInFlowScreenHandler::~AssistantOptInFlowScreenHandler() {
  if (assistant::AssistantSettings::Get() && voice_match_enrollment_started_)
    StopSpeakerIdEnrollment();
  if (AssistantState::Get())
    AssistantState::Get()->RemoveObserver(this);
}

void AssistantOptInFlowScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("assistantLogo", IDS_ASSISTANT_LOGO);
  builder->Add("assistantOptinLoading", IDS_ASSISTANT_OPT_IN_LOADING);
  builder->Add("assistantOptinLoadErrorTitle",
               IDS_ASSISTANT_OPT_IN_LOAD_ERROR_TITLE);
  builder->Add("assistantOptinLoadErrorMessage",
               IDS_ASSISTANT_OPT_IN_LOAD_ERROR_MESSAGE);
  builder->Add("assistantOptinSkipButton", IDS_ASSISTANT_OPT_IN_SKIP_BUTTON);
  builder->Add("assistantOptinRetryButton", IDS_ASSISTANT_OPT_IN_RETRY_BUTTON);
  builder->Add("assistantUserImage", IDS_ASSISTANT_OOBE_USER_IMAGE);
  builder->Add("assistantRelatedInfoTitle",
               IDS_ASSISTANT_RELATED_INFO_SCREEN_TITLE);
  builder->Add("assistantRelatedInfoTitleForChild",
               IDS_ASSISTANT_RELATED_INFO_SCREEN_TITLE_CHILD);
  builder->Add("assistantRelatedInfoMessage",
               IDS_ASSISTANT_RELATED_INFO_SCREEN_MESSAGE);
  builder->Add("assistantRelatedInfoReturnedUserTitle",
               IDS_ASSISTANT_RELATED_INFO_SCREEN_RETURNED_USER_TITLE);
  builder->Add("assistantRelatedInfoReturnedUserMessage",
               IDS_ASSISTANT_RELATED_INFO_SCREEN_RETURNED_USER_MESSAGE);
  builder->Add("assistantRelatedInfoReturnedUserMessageForChild",
               IDS_ASSISTANT_RELATED_INFO_SCREEN_RETURNED_USER_MESSAGE_CHILD);
  builder->Add("assistantRelatedInfoExample",
               IDS_ASSISTANT_RELATED_INFO_SCREEN_EXAMPLE);
  builder->Add("assistantRelatedInfoExampleForChild",
               IDS_ASSISTANT_RELATED_INFO_SCREEN_EXAMPLE_CHILD);
  builder->Add("assistantScreenContextTitle",
               IDS_ASSISTANT_SCREEN_CONTEXT_TITLE);
  builder->Add("assistantScreenContextDesc", IDS_ASSISTANT_SCREEN_CONTEXT_DESC);
  builder->Add("assistantScreenContextDescForChild",
               IDS_ASSISTANT_SCREEN_CONTEXT_DESC_CHILD);
  builder->Add("assistantVoiceMatchTitle", IDS_ASSISTANT_VOICE_MATCH_TITLE);
  builder->Add("assistantVoiceMatchTitleForChild",
               IDS_ASSISTANT_VOICE_MATCH_TITLE_CHILD);
  builder->AddF("assistantVoiceMatchMessage", IDS_ASSISTANT_VOICE_MATCH_MESSAGE,
                IsHotwordDspAvailable() || !DeviceHasBattery()
                    ? IDS_ASSISTANT_VOICE_MATCH_NOTICE_MESSAGE
                    : IDS_ASSISTANT_VOICE_MATCH_NO_DSP_NOTICE_MESSAGE);
  // Keep the child name placeholder as `$1`, so it could be set correctly
  // after user logs in.
  builder->AddF(
      "assistantVoiceMatchMessageForChild",
      IDS_ASSISTANT_VOICE_MATCH_MESSAGE_CHILD, u"$1",
      ui::GetChromeOSDeviceName(),
      IsHotwordDspAvailable() || !DeviceHasBattery()
          ? l10n_util::GetStringUTF16(
                IDS_ASSISTANT_VOICE_MATCH_NOTICE_MESSAGE_CHILD)
          : l10n_util::GetStringUTF16(
                IDS_ASSISTANT_VOICE_MATCH_NO_DSP_NOTICE_MESSAGE_CHILD));
  builder->Add("assistantVoiceMatchRecording",
               IDS_ASSISTANT_VOICE_MATCH_RECORDING);
  builder->Add("assistantVoiceMatchRecordingForChild",
               IDS_ASSISTANT_VOICE_MATCH_RECORDING_CHILD);
  builder->Add("assistantVoiceMatchCompleted",
               IDS_ASSISTANT_VOICE_MATCH_COMPLETED);
  builder->Add("assistantVoiceMatchFooter", IDS_ASSISTANT_VOICE_MATCH_FOOTER);
  builder->Add("assistantVoiceMatchFooterForChild",
               IDS_ASSISTANT_VOICE_MATCH_FOOTER_CHILD);
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
  builder->Add("assistantVoiceMatchAlreadySetupMessageForChild",
               IDS_ASSISTANT_VOICE_MATCH_ALREADY_SETUP_MESSAGE_CHILD);
  builder->Add("assistantOptinOKButton", IDS_OOBE_OK_BUTTON_TEXT);
  builder->Add("assistantOptinNoThanksButton", IDS_ASSISTANT_NO_THANKS_BUTTON);
  builder->Add("assistantOptinLaterButton", IDS_ASSISTANT_LATER_BUTTON);
  builder->Add("assistantOptinAgreeButton", IDS_ASSISTANT_AGREE_BUTTON);
  builder->Add("assistantOptinSaveButton", IDS_ASSISTANT_SAVE_BUTTON);
  builder->Add("assistantOptinWaitMessage", IDS_ASSISTANT_WAIT_MESSAGE);
  builder->Add("assistantReadyButton", IDS_ASSISTANT_DONE_BUTTON);
  builder->Add("back", IDS_EULA_BACK_BUTTON);
  builder->Add("next", IDS_EULA_NEXT_BUTTON);
  builder->Add("assistantOobePopupOverlayLoading",
               IDS_ASSISTANT_OOBE_POPUP_OVERLAY_LOADING);
  builder->Add("playAnimationAriaLabel", IDS_OOBE_PLAY_ANIMATION_MESSAGE);
  builder->Add("pauseAnimationAriaLabel", IDS_OOBE_PAUSE_ANIMATION_MESSAGE);
}

void AssistantOptInFlowScreenHandler::DeclareJSCallbacks() {
  AddCallback(
      "login.AssistantOptInFlowScreen.ValuePropScreen.userActed",
      &AssistantOptInFlowScreenHandler::HandleValuePropScreenUserAction);
  AddCallback(
      "login.AssistantOptInFlowScreen.RelatedInfoScreen.userActed",
      &AssistantOptInFlowScreenHandler::HandleRelatedInfoScreenUserAction);
  AddCallback(
      "login.AssistantOptInFlowScreen.VoiceMatchScreen.userActed",
      &AssistantOptInFlowScreenHandler::HandleVoiceMatchScreenUserAction);
  AddCallback("login.AssistantOptInFlowScreen.ValuePropScreen.screenShown",
              &AssistantOptInFlowScreenHandler::HandleValuePropScreenShown);
  AddCallback("login.AssistantOptInFlowScreen.RelatedInfoScreen.screenShown",
              &AssistantOptInFlowScreenHandler::HandleRelatedInfoScreenShown);
  AddCallback("login.AssistantOptInFlowScreen.VoiceMatchScreen.screenShown",
              &AssistantOptInFlowScreenHandler::HandleVoiceMatchScreenShown);
  AddCallback("login.AssistantOptInFlowScreen.timeout",
              &AssistantOptInFlowScreenHandler::HandleLoadingTimeout);
  AddCallback("login.AssistantOptInFlowScreen.flowFinished",
              &AssistantOptInFlowScreenHandler::HandleFlowFinished);
  AddCallback("login.AssistantOptInFlowScreen.initialized",
              &AssistantOptInFlowScreenHandler::HandleFlowInitialized);
}

void AssistantOptInFlowScreenHandler::GetAdditionalParameters(
    base::Value::Dict* dict) {
  dict->Set("voiceMatchDisabled", assistant::features::IsVoiceMatchDisabled());
  dict->Set("assistantLocale", g_browser_process->GetApplicationLocale());
  BaseScreenHandler::GetAdditionalParameters(dict);
}

void AssistantOptInFlowScreenHandler::Show() {
  SetupAssistantConnection();

  ShowInWebUI();
}

base::WeakPtr<AssistantOptInFlowScreenView>
AssistantOptInFlowScreenHandler::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AssistantOptInFlowScreenHandler::OnListeningHotword() {
  CallExternalAPI("onVoiceMatchUpdate", "listen");
}

void AssistantOptInFlowScreenHandler::OnProcessingHotword() {
  CallExternalAPI("onVoiceMatchUpdate", "process");
}

void AssistantOptInFlowScreenHandler::OnSpeakerIdEnrollmentDone() {
  StopSpeakerIdEnrollment();
  CallExternalAPI("onVoiceMatchUpdate", "done");
}

void AssistantOptInFlowScreenHandler::OnSpeakerIdEnrollmentFailure() {
  StopSpeakerIdEnrollment();
  RecordAssistantOptInStatus(
      AssistantOptInFlowStatus::kVoiceMatchEnrollmentError);
  voice_match_enrollment_error_ = true;
  CallExternalAPI("onVoiceMatchUpdate", "failure");
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
  prefs->SetBoolean(assistant::prefs::kAssistantEnabled, true);

  if (AssistantState::Get()->assistant_status() ==
      assistant::AssistantStatus::NOT_READY) {
    AssistantState::Get()->AddObserver(this);
  } else {
    SendGetSettingsRequest();
  }
}

void AssistantOptInFlowScreenHandler::ShowNextScreen() {
  CallExternalAPI("showNextScreen");
}

void AssistantOptInFlowScreenHandler::OnActivityControlOptInResult(
    bool opted_in) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(!pending_consent_data_.empty());
  auto data = pending_consent_data_.front();
  pending_consent_data_.pop_front();
  RecordActivityControlConsent(profile, data.ui_audit_key, opted_in,
                               data.setting_type);
  if (opted_in) {
    has_opted_in_any_consent_ = true;
    RecordAssistantActivityControlOptInStatus(data.setting_type, opted_in);
    CHECK(assistant::AssistantSettings::Get());
    assistant::AssistantSettings::Get()->UpdateSettings(
        GetSettingsUiUpdate(data.consent_token).SerializeAsString(),
        base::BindOnce(
            &AssistantOptInFlowScreenHandler::OnUpdateSettingsResponse,
            weak_factory_.GetWeakPtr()));
  } else {
    has_opted_out_any_consent_ = true;
    RecordAssistantActivityControlOptInStatus(data.setting_type, opted_in);
    profile->GetPrefs()->SetInteger(assistant::prefs::kAssistantConsentStatus,
                                    assistant::prefs::ConsentStatus::kUnknown);
    if (pending_consent_data_.empty()) {
      if (has_opted_in_any_consent_) {
        ShowNextScreen();
      } else {
        HandleFlowFinished();
      }
    } else {
      UpdateValuePropScreen();
    }
  }
}

void AssistantOptInFlowScreenHandler::OnScreenContextOptInResult(
    bool opted_in) {
  RecordAssistantOptInStatus(
      opted_in ? AssistantOptInFlowStatus::kRelatedInfoAccepted
               : AssistantOptInFlowStatus::kRelatedInfoSkipped);
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(assistant::prefs::kAssistantContextEnabled, opted_in);
}

void AssistantOptInFlowScreenHandler::OnDialogClosed() {
  // Disable hotword for user if voice match enrollment has not completed.
  // No need to disable for retrain flow since user has a model.
  // No need to disable if there's error during the enrollment.
  if (!voice_match_enrollment_done_ && !voice_match_enrollment_error_ &&
      flow_type_ == FlowType::kSpeakerIdEnrollment) {
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
        assistant::prefs::kAssistantHotwordEnabled, false);
  }
}

void AssistantOptInFlowScreenHandler::OnAssistantSettingsEnabled(bool enabled) {
  // Close the opt-in screen is the Assistant is disabled.
  if (!enabled)
    HandleFlowFinished();
}

void AssistantOptInFlowScreenHandler::OnAssistantStatusChanged(
    assistant::AssistantStatus status) {
  if (status != assistant::AssistantStatus::NOT_READY) {
    SendGetSettingsRequest();
    AssistantState::Get()->RemoveObserver(this);
  }
}

void AssistantOptInFlowScreenHandler::SendGetSettingsRequest() {
  if (!initialized_)
    return;

  if (AssistantState::Get()->assistant_status() ==
      assistant::AssistantStatus::NOT_READY) {
    return;
  }

  assistant::SettingsUiSelector selector = GetSettingsUiSelector();
  CHECK(assistant::AssistantSettings::Get());
  assistant::AssistantSettings::Get()->GetSettingsWithHeader(
      selector.SerializeAsString(),
      base::BindOnce(&AssistantOptInFlowScreenHandler::OnGetSettingsResponse,
                     weak_factory_.GetWeakPtr()));
  send_request_time_ = base::TimeTicks::Now();
}

void AssistantOptInFlowScreenHandler::StopSpeakerIdEnrollment() {
  DCHECK(voice_match_enrollment_started_);
  voice_match_enrollment_started_ = false;

  if (AssistantState::Get()->settings_enabled().value_or(false) &&
      AssistantState::Get()->assistant_status() ==
          assistant::AssistantStatus::READY) {
    CHECK(assistant::AssistantSettings::Get());
    assistant::AssistantSettings::Get()->StopSpeakerIdEnrollment();
  }
  // Reset the mojom receiver of |SpeakerIdEnrollmentClient|.
  ResetReceiver();
}

void AssistantOptInFlowScreenHandler::ReloadContent(base::Value::Dict dict) {
  CallExternalAPI("reloadContent", std::move(dict));
}

void AssistantOptInFlowScreenHandler::AddSettingZippy(const std::string& type,
                                                      base::Value::List data) {
  CallExternalAPI("addSettingZippy", type, std::move(data));
}

void AssistantOptInFlowScreenHandler::UpdateValuePropScreen() {
  CallExternalAPI("onValuePropUpdate");
}

void AssistantOptInFlowScreenHandler::OnGetSettingsResponse(
    const std::string& response) {
  const base::TimeDelta time_since_request_sent =
      base::TimeTicks::Now() - send_request_time_;
  UMA_HISTOGRAM_TIMES("Assistant.OptInFlow.GetSettingsRequestTime",
                      time_since_request_sent);

  if (ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          assistant::prefs::kAssistantDisabledByPolicy)) {
    DVLOG(1) << "Assistant is disabled by domain policy. Skip Assistant "
                "opt-in flow.";
    HandleFlowFinished();
    return;
  }

  assistant::GetSettingsUiResponse settings_ui_response;
  if (!settings_ui_response.ParseFromString(response)) {
    LOG(ERROR) << "Failed to parse get settings response.";
    HandleFlowFinished();
    return;
  }

  auto settings_ui = settings_ui_response.settings();
  auto header = settings_ui_response.header();

  bool equal_weight_buttons =
      header.footer_button_layout() ==
      assistant::SettingsResponseHeader_AcceptRejectLayout_EQUAL_WEIGHT;

  if (settings_ui.has_gaia_user_context_ui()) {
    auto gaia_user_context_ui = settings_ui.gaia_user_context_ui();
    if (gaia_user_context_ui.assistant_disabled_by_dasher_domain()) {
      DVLOG(1) << "Assistant is disabled by domain policy. Skip Assistant "
                  "opt-in flow.";
      PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
      prefs->SetBoolean(assistant::prefs::kAssistantDisabledByPolicy, true);
      prefs->SetBoolean(assistant::prefs::kAssistantEnabled, false);
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

  RecordAssistantOptInStatus(AssistantOptInFlowStatus::kFlowStarted);

  base::Value::List zippy_data;
  bool skip_activity_control = true;
  pending_consent_data_.clear();
  // We read from `multi_consent_ui` field if it is populated and we assume user
  // is a minor user; otherwise, we read from `consent_ui` field.
  if (settings_ui.consent_flow_ui().multi_consent_ui().size()) {
    auto multi_consent_ui = settings_ui.consent_flow_ui().multi_consent_ui();
    for (auto consent_ui : multi_consent_ui) {
      auto activity_control_ui = consent_ui.activity_control_ui();
      if (activity_control_ui.setting_zippy().size()) {
        skip_activity_control = false;
        auto data = ConsentData();
        data.consent_token = activity_control_ui.consent_token();
        data.ui_audit_key = activity_control_ui.ui_audit_key();
        data.setting_type = GetActivityControlConsentSettingType(
            activity_control_ui.setting_zippy());
        pending_consent_data_.push_back(data);
        zippy_data.Append(
            CreateZippyData(activity_control_ui, /*is_minor_mode=*/true));
      }
    }
  } else {
    auto consent_ui = settings_ui.consent_flow_ui().consent_ui();
    auto activity_control_ui = consent_ui.activity_control_ui();
    if (activity_control_ui.setting_zippy().size()) {
      skip_activity_control = false;
      auto data = ConsentData();
      data.consent_token = activity_control_ui.consent_token();
      data.ui_audit_key = activity_control_ui.ui_audit_key();
      data.setting_type =
          sync_pb::UserConsentTypes::AssistantActivityControlConsent::ALL;
      pending_consent_data_.push_back(data);
      zippy_data.Append(
          CreateZippyData(activity_control_ui, /*is_minor_mode=*/false));
    }
  }

  // Process activity control data.
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
    AddSettingZippy("settings", std::move(zippy_data));
  }

  const bool is_oobe_in_progress =
      session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE;
  // Pass string constants dictionary.
  auto dictionary = GetSettingsUiStrings(settings_ui, activity_control_needed_,
                                         equal_weight_buttons);
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  dictionary.Set("voiceMatchEnforcedOff",
                 IsVoiceMatchEnforcedOff(prefs, is_oobe_in_progress));
  dictionary.Set("shouldSkipVoiceMatch",
                 !AssistantState::Get()->HasAudioInputDevice());
  dictionary.Set("childName", GetGivenNameIfIsChild());
  dictionary.Set(
      "isTabletMode",
      display::Screen::GetScreen()->InTabletMode() ||
          (is_oobe_in_progress && switches::ShouldOobeUseTabletModeFirstRun()));
  ReloadContent(std::move(dictionary));

  // Skip activity control and users will be in opted out mode.
  if (skip_activity_control)
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
      LOG(ERROR) << "Consent update error.";
    } else if (activity_control_needed_ && pending_consent_data_.empty() &&
               !has_opted_out_any_consent_) {
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
      LOG(ERROR) << "Email OptIn update error.";
    }
    return;
  }

  if (pending_consent_data_.empty()) {
    ShowNextScreen();
  } else {
    UpdateValuePropScreen();
  }
}

void AssistantOptInFlowScreenHandler::HandleValuePropScreenUserAction(
    const std::string& action) {
  if (action == kSkipPressed) {
    OnActivityControlOptInResult(false);
  } else if (action == kNextPressed) {
    OnActivityControlOptInResult(true);
  } else if (action == kReloadRequested) {
    SendGetSettingsRequest();
  }
}

void AssistantOptInFlowScreenHandler::HandleRelatedInfoScreenUserAction(
    const std::string& action) {
  if (action == kSkipPressed) {
    OnScreenContextOptInResult(false);
    ShowNextScreen();
  } else if (action == kNextPressed) {
    OnScreenContextOptInResult(true);
    ShowNextScreen();
  }
}

void AssistantOptInFlowScreenHandler::HandleVoiceMatchScreenUserAction(
    const std::string& action) {
  // If the Assistant is disabled or is not ready, discard the action and end
  // the flow instead.
  if (!AssistantState::Get()->settings_enabled().value_or(false) ||
      AssistantState::Get()->assistant_status() ==
          assistant::AssistantStatus::NOT_READY) {
    HandleFlowFinished();
    return;
  }

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();

  if (action == kVoiceMatchDone) {
    RecordAssistantOptInStatus(
        AssistantOptInFlowStatus::kVoiceMatchEnrollmentDone);
    voice_match_enrollment_done_ = true;
    voice_match_enrollment_error_ = false;
    ShowNextScreen();
  } else if (action == kSkipPressed) {
    RecordAssistantOptInStatus(
        AssistantOptInFlowStatus::kVoiceMatchEnrollmentSkipped);
    // Disable hotword for user if voice match enrollment has not completed.
    // No need to disable for retrain flow since user has a model.
    // No need to disable if there's error during the enrollment.
    if (flow_type_ != FlowType::kSpeakerIdRetrain &&
        !voice_match_enrollment_error_) {
      prefs->SetBoolean(assistant::prefs::kAssistantHotwordEnabled, false);
    }
    if (voice_match_enrollment_started_)
      StopSpeakerIdEnrollment();
    ShowNextScreen();
  } else if (action == kRecordPressed) {
    if (!prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled)) {
      prefs->SetBoolean(assistant::prefs::kAssistantHotwordEnabled, true);
    }

    DCHECK(!voice_match_enrollment_started_);
    voice_match_enrollment_started_ = true;
    CHECK(assistant::AssistantSettings::Get());
    assistant::AssistantSettings::Get()->StartSpeakerIdEnrollment(
        flow_type_ == FlowType::kSpeakerIdRetrain, weak_factory_.GetWeakPtr());
  } else if (action == kReloadRequested) {
    if (voice_match_enrollment_started_)
      StopSpeakerIdEnrollment();
  }
}

void AssistantOptInFlowScreenHandler::HandleValuePropScreenShown() {
  RecordAssistantOptInStatus(AssistantOptInFlowStatus::kActivityControlShown);
}

void AssistantOptInFlowScreenHandler::HandleRelatedInfoScreenShown() {
  RecordAssistantOptInStatus(AssistantOptInFlowStatus::kRelatedInfoShown);
}

void AssistantOptInFlowScreenHandler::HandleVoiceMatchScreenShown() {
  RecordAssistantOptInStatus(AssistantOptInFlowStatus::kVoiceMatchShown);
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
  base::Value::List args;
  args.Append(kFlowFinished);
  if (!HandleUserActionImpl(args)) {
    CallExternalAPI("closeDialog");
  }
}

void AssistantOptInFlowScreenHandler::HandleFlowInitialized(
    const int flow_type) {
  // Allow JavaScript. This is necessary for the in-session WebUI and is is not
  // triggered in the OOBE flow, where `screen` objects are associated with
  // handlers. TODO(crbug.com/1309022) - Separate in-session and OOBE handlers.
  if (!is_oobe_)
    AllowJavascript();

  auto* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  // Do not skip the flow if the OOBE debug overlay is present. Otherwise it is
  // not possible to test the screen manually.
  const bool debugger_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowOobeDevOverlay);
  if (!prefs->GetBoolean(assistant::prefs::kAssistantEnabled) &&
      !debugger_enabled) {
    HandleFlowFinished();
    return;
  }

  initialized_ = true;

  DCHECK(IsKnownEnumValue(static_cast<FlowType>(flow_type)));
  flow_type_ = static_cast<FlowType>(flow_type);

  if (flow_type_ == FlowType::kConsentFlow)
    SendGetSettingsRequest();
}

bool AssistantOptInFlowScreenHandler::DeviceHasBattery() {
  // Assume that the device has a battery if we can't determine otherwise.
  if (!chromeos::PowerManagerClient::Get())
    return true;

  auto status = chromeos::PowerManagerClient::Get()->GetLastStatus();
  if (!status.has_value() || !status->has_battery_state())
    return true;

  return status->battery_state() !=
         power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT;
}

}  // namespace ash
