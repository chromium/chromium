// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/testapi/oobe_test_api_handler.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/ash_interfaces.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ash/login/screens/split_modifier_keyboard_info_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/login/localized_values_builder.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/user_manager/user_manager.h"
#include "services/device/public/mojom/input_service.mojom.h"
#include "ui/display/screen.h"

namespace ash {

OobeTestAPIHandler::OobeTestAPIHandler() = default;
OobeTestAPIHandler::~OobeTestAPIHandler() = default;

void OobeTestAPIHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("testapi_browseAsGuest", IDS_ASH_BROWSE_AS_GUEST_BUTTON);
}

void OobeTestAPIHandler::DeclareJSCallbacks() {
  AddCallback("OobeTestApi.loginWithPin", &OobeTestAPIHandler::LoginWithPin);
  AddCallback("OobeTestApi.advanceToScreen",
              &OobeTestAPIHandler::AdvanceToScreen);
  AddCallback("OobeTestApi.skipToLoginForTesting",
              &OobeTestAPIHandler::SkipToLoginForTesting);
  AddCallback("OobeTestApi.skipPostLoginScreens",
              &OobeTestAPIHandler::SkipPostLoginScreens);
  AddCallback("OobeTestApi.completeLogin",
              &OobeTestAPIHandler::HandleCompleteLogin);
  AddCallback("OobeTestApi.loginAsGuest", &OobeTestAPIHandler::LoginAsGuest);
  AddCallback("OobeTestApi.showGaiaDialog",
              &OobeTestAPIHandler::ShowGaiaDialog);

  // Keeping the code in case the test using this will be ported to tast. The
  // function used to be called getPrimaryDisplayNameForTesting. In order to use
  // this one you need to add a function into login/test_api/test_api.js.
  AddCallback("OobeTestApi.getPrimaryDisplayName",
              &OobeTestAPIHandler::HandleGetPrimaryDisplayName);

  AddCallback("OobeTestApi.getShouldSkipChoobe",
              &OobeTestAPIHandler::HandleGetShouldSkipChoobe);
  AddCallback("OobeTestApi.getShouldSkipTouchpadScroll",
              &OobeTestAPIHandler::HandleGetShouldSkipTouchpadScroll);
  AddCallback("OobeTestApi.getMetricsClientID",
              &OobeTestAPIHandler::HandleGetMetricsClientID);
}

void OobeTestAPIHandler::GetAdditionalParameters(base::Value::Dict* dict) {
  login::NetworkStateHelper helper_;
  dict->Set("testapi_shouldSkipNetworkFirstShow",
                !switches::IsOOBENetworkScreenSkippingDisabledForTesting() &&
                helper_.IsConnectedToEthernet());

  dict->Set(
      "testapi_shouldSkipGuestTos",
      StartupUtils::IsEulaAccepted() || !BUILDFLAG(GOOGLE_CHROME_BRANDING));

  dict->Set("testapi_isFingerprintSupported",
            quick_unlock::IsFingerprintSupported());

  dict->Set("testapi_shouldSkipAiIntro", AiIntroScreen::ShouldBeSkipped());

  dict->Set("testapi_shouldSkipGeminiIntro",
            GeminiIntroScreen::ShouldBeSkipped());

  dict->Set("testapi_shouldSkipSplitModifierKeyboardInfo",
            SplitModifierKeyboardInfoScreen::ShouldBeSkipped());

  dict->Set("testapi_shouldSkipAssistant",
            features::IsOobeSkipAssistantEnabled() ||
                !BUILDFLAG(ENABLE_CROS_LIBASSISTANT));

  dict->Set("testapi_isBrandedBuild",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            true
#else
            false
#endif
  );

  dict->Set("testapi_isOobeInTabletMode",
            display::Screen::GetScreen()->InTabletMode() ||
                switches::ShouldOobeUseTabletModeFirstRun());
  dict->Set("testapi_shouldSkipConsolidatedConsent",
            !BUILDFLAG(GOOGLE_CHROME_BRANDING));
  dict->Set("testapi_isHPSEnabled", ash::features::IsQuickDimEnabled());
  dict->Set("testapi_shouldSkipDisplaySize",
            !features::IsOobeDisplaySizeEnabled());
  dict->Set("testapi_shouldSkipGaiaInfoScreen",
            !features::IsOobeGaiaInfoScreenEnabled());
  dict->Set("testapi_isOobeQuickStartEnabled",
            features::IsOobeQuickStartEnabled());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // The current method is called early, before the user logs-in,
  // If Chrome was launched in OOBE, `is_owner` will be set to true since
  // `user_manager->GetUsers().size()` would return 0.
  // If it's launched in the login screen to test the add person flow, then
  // the number of existing users before the new user logs-in should be > 0.
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  auto* user_manager = user_manager::UserManager::Get();
  bool is_owner = !connector->IsDeviceEnterpriseManaged() &&
                  user_manager->GetUsers().size() == 0;
  dict->Set("testapi_shouldSkipHwDataCollection",
            !is_owner || !switches::IsRevenBranding());
#else
  dict->Set("testapi_shouldSkipHwDataCollection", true);
#endif
}

void OobeTestAPIHandler::LoginWithPin(const std::string& username,
                                      const std::string& pin) {
  VLOG(1) << "LoginWithPin";
  LoginScreenClientImpl::Get()->AuthenticateUserWithPasswordOrPin(
      AccountId::FromUserEmail(username), pin, /*authenticated_by_pin=*/true,
      base::BindOnce([](bool success) {
        LOG_IF(ERROR, !success) << "Failed to authenticate with pin";
      }));
}

void OobeTestAPIHandler::AdvanceToScreen(const std::string& screen) {
  VLOG(1) << "AdvanceToScreen(" << screen << ")";
  LoginDisplayHost::default_host()->StartWizard(OobeScreenId(screen));
}

void OobeTestAPIHandler::SkipToLoginForTesting() {
  VLOG(1) << "SkipToLoginForTesting";
  WizardController* controller = WizardController::default_controller();
  if (!controller || !controller->is_initialized()) {
    LOG(ERROR)
        << "SkipToLoginForTesting is called when WizardController is not yet "
           "initialized. Please report at https://crbug.com/1336940";
    return;
  }
  controller->SkipToLoginForTesting();  // IN-TEST
}

void OobeTestAPIHandler::SkipPostLoginScreens() {
  VLOG(1) << "SkipPostLoginScreens";
  WizardController::default_controller()
      ->SkipPostLoginScreensForTesting();  // IN-TEST
}

void OobeTestAPIHandler::HandleCompleteLogin(const std::string& gaia_id,
                                             const std::string& typed_email,
                                             const std::string& password) {
  VLOG(1) << __func__;
  DCHECK(!typed_email.empty());
  DCHECK(!gaia_id.empty());
  const std::string sanitized_email = gaia::SanitizeEmail(typed_email);
  LoginDisplayHost::default_host()->SetDisplayEmail(sanitized_email);
  const AccountId account_id =
      login::GetAccountId(typed_email, gaia_id, AccountType::GOOGLE);
  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);

  std::unique_ptr<UserContext> user_context =
      login::BuildUserContextForGaiaSignIn(
          user ? user->GetType() : user_manager::UserType::kRegular,
          login::GetAccountId(typed_email, gaia_id, AccountType::GOOGLE),
          /*using_saml=*/false, /*using_saml_api=*/false, password,
          SamlPasswordAttributes(),
          /*sync_trusted_vault_keys=*/std::nullopt,
          /*challenge_response_key=*/std::nullopt);

  LoginDisplayHost::default_host()->CompleteLogin(*user_context);
}

void OobeTestAPIHandler::LoginAsGuest() {
  VLOG(1) << "LoginAsGuest";
  WizardController::default_controller()->SkipToLoginForTesting();  // IN-TEST
  CHECK(ExistingUserController::current_controller());
  UserContext context(user_manager::UserType::kGuest, EmptyAccountId());
  ExistingUserController::current_controller()->Login(context,
                                                      SigninSpecifics());
}

void OobeTestAPIHandler::ShowGaiaDialog() {
  VLOG(1) << "ShowGaiaDialog";
  LoginDisplayHost::default_host()->ShowGaiaDialog(EmptyAccountId());
}

void OobeTestAPIHandler::HandleGetPrimaryDisplayName(
    const std::string& callback_id) {
  mojo::Remote<crosapi::mojom::CrosDisplayConfigController> cros_display_config;
  BindCrosDisplayConfigController(
      cros_display_config.BindNewPipeAndPassReceiver());

  cros_display_config->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(&OobeTestAPIHandler::OnGetDisplayUnitInfoList,
                     base::Unretained(this), callback_id));
}

void OobeTestAPIHandler::OnGetDisplayUnitInfoList(
    const std::string& callback_id,
    std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list) {
  std::string display_name;
  for (const crosapi::mojom::DisplayUnitInfoPtr& info : info_list) {
    if (info->is_primary) {
      display_name = info->name;
      break;
    }
  }
  if (display_name.empty()) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value(display_name));
    return;
  }
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(display_name));
}

void OobeTestAPIHandler::HandleGetShouldSkipChoobe(
    const std::string& callback_id) {
  // CHOOBE screen is only skipped if the number of optional screens is less
  // than 3, since theme selection is always shown, CHOOBE should be skipped
  // when display size Screen or touchpad scroll screen is skipped.
  bool skip_touchpad_scroll =
      !features::IsOobeTouchpadScrollEnabled() ||
      InputDeviceSettingsController::Get()->GetConnectedTouchpads().empty();
  bool skip_display_size = !features::IsOobeDisplaySizeEnabled();

  ResolveJavascriptCallback(base::Value(callback_id),
                            !features::IsOobeChoobeEnabled() ||
                                skip_touchpad_scroll || skip_display_size);
}

void OobeTestAPIHandler::HandleGetShouldSkipTouchpadScroll(
    const std::string& callback_id) {
  ResolveJavascriptCallback(base::Value(callback_id),
                            !features::IsOobeTouchpadScrollEnabled() ||
                                InputDeviceSettingsController::Get()
                                    ->GetConnectedTouchpads()
                                    .empty());
}

void OobeTestAPIHandler::HandleGetMetricsClientID(
    const std::string& callback_id) {
  std::string client_id;
  if (g_browser_process->metrics_service()) {
    client_id = g_browser_process->metrics_service()->GetClientId();
  }

  // Early in OOBE `metrics_service()->GetClientId()` will return an empty
  // string. If that's the case look for the client ID in the preference
  // `kMetricsProvisionalClientID`.
  if (client_id.empty()) {
    client_id = g_browser_process->local_state()->GetString(
        metrics::prefs::kMetricsProvisionalClientID);
  }
  ResolveJavascriptCallback(base::Value(callback_id), client_id);
}

}  // namespace ash
