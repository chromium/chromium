// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace chromeos {

BaseScreenHandler::BaseScreenHandler(OobeScreenId oobe_screen,
                                     JSCallsContainer* js_calls_container)
    : BaseWebUIHandler(js_calls_container), oobe_screen_(oobe_screen) {}

BaseScreenHandler::~BaseScreenHandler() = default;

void BaseScreenHandler::SetBaseScreen(BaseScreen* base_screen) {
  if (base_screen_ == base_screen)
    return;
  base_screen_ = base_screen;
}

void BaseScreenHandler::RegisterMessages() {
  if (!user_acted_method_path_.empty()) {
    AddCallback(user_acted_method_path_, &BaseScreenHandler::HandleUserAction);
  }

  BaseWebUIHandler::RegisterMessages();
}

void BaseScreenHandler::HandleUserAction(const std::string& action_id) {
  if (base_screen_)
    base_screen_->OnUserAction(action_id);
}

}  // namespace chromeos
