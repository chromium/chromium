// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/locale_switch_screen_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/login/screens/locale_switch_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/js_calls_container.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace chromeos {

constexpr StaticOobeScreenId LocaleSwitchView::kScreenId;

LocaleSwitchScreenHandler::LocaleSwitchScreenHandler(
    JSCallsContainer* js_calls_container,
    CoreOobeView* core_oobe_view)
    : BaseScreenHandler(kScreenId, js_calls_container),
      core_oobe_view_(core_oobe_view) {}

LocaleSwitchScreenHandler::~LocaleSwitchScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void LocaleSwitchScreenHandler::Bind(LocaleSwitchScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
  screen_ = screen;
}

void LocaleSwitchScreenHandler::Unbind() {
  BaseScreenHandler::SetBaseScreen(nullptr);
  screen_ = nullptr;
}

void LocaleSwitchScreenHandler::UpdateStrings() {
  base::DictionaryValue localized_strings;
  GetOobeUI()->GetLocalizedStrings(&localized_strings);
  core_oobe_view_->ReloadContent(localized_strings);
}

void LocaleSwitchScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void LocaleSwitchScreenHandler::Initialize() {}

}  // namespace chromeos
