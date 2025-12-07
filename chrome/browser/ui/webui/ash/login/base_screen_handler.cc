// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"

namespace ash {

namespace {
constexpr char kLoginPrefix[] = "login.";
constexpr char kUserActedCallback[] = ".userActed";
}  // namespace

BaseScreenHandler::BaseScreenHandler(OobeScreenId oobe_screen)
    : oobe_screen_(oobe_screen) {
  DCHECK_NE(oobe_screen_.name, OOBE_SCREEN_UNKNOWN.name);
  DCHECK(!oobe_screen_.external_api_prefix.empty());
  user_acted_method_path_ = base::StrCat(
      {kLoginPrefix, oobe_screen_.external_api_prefix, kUserActedCallback});
}

BaseScreenHandler::~BaseScreenHandler() = default;

void BaseScreenHandler::ShowInWebUI(std::optional<base::Value::Dict> data) {
  if (!GetOobeUI()) {
    return;
  }
  GetOobeUI()->GetCoreOobe()->ShowScreenWithData(oobe_screen_, std::move(data));
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
  HandleUserActionImpl(args);
}

bool BaseScreenHandler::HandleUserActionImpl(const base::Value::List& args) {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  if (!host) {
    return false;
  }

  WizardController* wizard_controller = host->GetWizardController();

  // TODO(b/345711957): Upgrade to `CHECK()` and remove the handling of the case
  // when `wizard_controller` is null.
  DUMP_WILL_BE_CHECK(wizard_controller);
  if (!wizard_controller) {
    return false;
  }

  BaseScreen* screen = wizard_controller->GetScreen(oobe_screen_);

  // TODO(b/345711957): Upgrade to `CHECK()` and remove the handling of the case
  // when `screen` is null.
  DUMP_WILL_BE_CHECK(screen);
  if (!screen) {
    return false;
  }

  screen->HandleUserAction(args);
  return true;
}

std::string BaseScreenHandler::GetFullExternalAPIFunctionName(
    const std::string& short_name) {
  DCHECK(!oobe_screen_.external_api_prefix.empty());
  return base::StrCat(
      {kLoginPrefix, oobe_screen_.external_api_prefix, ".", short_name});
}

}  // namespace ash
