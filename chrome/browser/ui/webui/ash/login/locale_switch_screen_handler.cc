// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/locale_switch_screen_handler.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/login/screens/locale_switch_screen.h"
#include "chrome/browser/ui/webui/ash/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"

namespace ash {

LocaleSwitchScreenHandler::LocaleSwitchScreenHandler(
    CoreOobeView* core_oobe_view)
    : BaseScreenHandler(kScreenId), core_oobe_view_(core_oobe_view) {}

LocaleSwitchScreenHandler::~LocaleSwitchScreenHandler() = default;

void LocaleSwitchScreenHandler::UpdateStrings() {
  base::Value::Dict localized_strings = GetOobeUI()->GetLocalizedStrings();
  core_oobe_view_->ReloadContent(std::move(localized_strings));
}

void LocaleSwitchScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

}  // namespace ash
