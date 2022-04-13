// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace chromeos {

namespace {
constexpr char kLoginPrefix[] = "login.";
constexpr char kUserActedCallback[] = ".userActed";
}  // namespace

BaseScreenHandler::BaseScreenHandler(OobeScreenId oobe_screen)
    : oobe_screen_(oobe_screen) {
  DCHECK_NE(oobe_screen_.name, ash::OOBE_SCREEN_UNKNOWN.name);
  if (!oobe_screen_.external_api_prefix.empty()) {
    user_acted_method_path_ = base::StrCat(
        {kLoginPrefix, oobe_screen_.external_api_prefix, kUserActedCallback});
  }
}

BaseScreenHandler::~BaseScreenHandler() = default;

void BaseScreenHandler::SetBaseScreenDeprecated(BaseScreen* base_screen) {
#if DCHECK_IS_ON()
  base_screen_ = base_screen;
  if (!base_screen) {
    // TODO(rsorokin): Insert check if LDH is finalizing here.
    return;
  }
#endif
}

void BaseScreenHandler::ShowInWebUI(absl::optional<base::Value::Dict> data) {
  if (!GetOobeUI())
    return;
  GetOobeUI()->GetCoreOobeView()->ShowScreenWithData(oobe_screen_,
                                                     std::move(data));
}

void BaseScreenHandler::RegisterMessages() {
  if (!user_acted_method_path_.empty()) {
    web_ui()->RegisterMessageCallback(
        user_acted_method_path_,
        base::BindRepeating(&BaseScreenHandler::HandleUserAction,
                            base::Unretained(this)));
  }

  BaseWebUIHandler::RegisterMessages();
}

void BaseScreenHandler::HandleUserAction(const base::Value::List& args) {
  if (!ash::LoginDisplayHost::default_host())
    return;

#if DCHECK_IS_ON()
  if (base_screen_) {
    DCHECK_EQ(
        ash::LoginDisplayHost::default_host()->GetWizardController()->GetScreen(
            oobe_screen_),
        base_screen_);
  }
#endif

  ash::LoginDisplayHost::default_host()
      ->GetWizardController()
      ->GetScreen(oobe_screen_)
      ->HandleUserAction(args);
}

std::string BaseScreenHandler::GetFullExternalAPIFunctionName(
    const std::string& short_name) {
  DCHECK(!oobe_screen_.external_api_prefix.empty());
  return base::StrCat(
      {kLoginPrefix, oobe_screen_.external_api_prefix, ".", short_name});
}

}  // namespace chromeos
