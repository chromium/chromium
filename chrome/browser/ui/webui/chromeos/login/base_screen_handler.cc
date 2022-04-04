// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace chromeos {

BaseScreenHandler::BaseScreenHandler(OobeScreenId oobe_screen)
    : oobe_screen_(oobe_screen) {
  DCHECK_NE(oobe_screen_.name, OobeScreen::SCREEN_UNKNOWN.name);
  if (!oobe_screen_.external_api_prefix.empty()) {
    user_acted_method_path_ = base::StrCat(
        {"login.", oobe_screen_.external_api_prefix, ".userActed"});
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
    AddCallback(user_acted_method_path_, &BaseScreenHandler::HandleUserAction);
  }

  BaseWebUIHandler::RegisterMessages();
}

void BaseScreenHandler::HandleUserAction(const std::string& action_id) {
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
      ->HandleUserAction(action_id);
}

}  // namespace chromeos
