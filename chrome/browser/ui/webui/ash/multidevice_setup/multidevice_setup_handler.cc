// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_handler.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/user_image/user_image_source.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_manager/user.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::multidevice_setup {

MultideviceSetupHandler::MultideviceSetupHandler()
    : auth_factor_editor_(UserDataAuthClient::Get()) {
  const user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(
      ProfileManager::GetActiveUserProfile());

  if (!user) {
    LOG(ERROR) << "User not found.";
    return;
  }

  auth_factor_editor_.GetAuthFactorsConfiguration(
      std::make_unique<UserContext>(*user),
      base::BindOnce(&MultideviceSetupHandler::OnGetAuthFactorsConfiguration,
                     base::Unretained(this)));
}

MultideviceSetupHandler::~MultideviceSetupHandler() = default;

void MultideviceSetupHandler::OnGetAuthFactorsConfiguration(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to get auth factors configuration, code "
               << error->get_cryptohome_error();
  } else {
    const auto& config = user_context->GetAuthFactorsConfiguration();
    // Allow authentication by PIN if that is the user's only auth factor.
    authenticate_by_pin_ =
        !config.HasConfiguredFactor(cryptohome::AuthFactorType::kPassword) &&
        config.HasConfiguredFactor(cryptohome::AuthFactorType::kPin);
    if (IsJavascriptAllowed() && authenticate_by_pin_) {
      FireWebUIListener("multidevice_setup.switchToPinAuth");
    }
  }
}

void MultideviceSetupHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getProfileInfo",
      base::BindRepeating(&MultideviceSetupHandler::HandleGetProfileInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openMultiDeviceSettings",
      base::BindRepeating(
          &MultideviceSetupHandler::HandleOpenMultiDeviceSettings,
          base::Unretained(this)));
}

void MultideviceSetupHandler::HandleGetProfileInfo(
    const base::Value::List& args) {
  AllowJavascript();

  DCHECK(!args.empty());
  std::string callback_id = args[0].GetString();

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(Profile::FromWebUI(web_ui()));

  base::Value::Dict response;
  response.Set("email", user->GetDisplayEmail());

  scoped_refptr<base::RefCountedMemory> image =
      UserImageSource::GetUserImage(user->GetAccountId());
  response.Set("profilePhotoUrl", webui::GetPngDataUrl(*image));
  response.Set("authenticateByPin", authenticate_by_pin_);

  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void MultideviceSetupHandler::HandleOpenMultiDeviceSettings(
    const base::Value::List& args) {
  DCHECK(args.empty());
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      Profile::FromWebUI(web_ui()),
      chromeos::settings::mojom::kMultiDeviceFeaturesSubpagePath);
}

}  // namespace ash::multidevice_setup
