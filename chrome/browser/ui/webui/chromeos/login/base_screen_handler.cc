// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace chromeos {

BaseScreenHandler::BaseScreenHandler(OobeScreenId oobe_screen,
                                     JSCallsContainer* js_calls_container)
    : BaseWebUIHandler(js_calls_container), oobe_screen_(oobe_screen) {
  DCHECK_NE(oobe_screen_.name, OobeScreen::SCREEN_UNKNOWN.name);
}

BaseScreenHandler::~BaseScreenHandler() = default;

void BaseScreenHandler::SetBaseScreen(BaseScreen* base_screen) {
  if (base_screen_ == base_screen)
    return;
  base_screen_ = base_screen;
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
  if (base_screen_)
    base_screen_->HandleUserAction(action_id);
}

}  // namespace chromeos
