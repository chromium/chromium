// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

PasswordSelectionScreenHandler::PasswordSelectionScreenHandler()
    : BaseScreenHandler(kScreenId) {}

PasswordSelectionScreenHandler::~PasswordSelectionScreenHandler() = default;

void PasswordSelectionScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("passwordSelectionTitle", IDS_PASSWORD_SELECTION_TITLE,
                ui::GetChromeOSDeviceName());
  builder->AddF("passwordSelectionSubtitile", IDS_PASSWORD_SELECTION_SUBTITLE,
                ui::GetChromeOSDeviceName());
  builder->AddF("localPasswordSelectionLabel",
                IDS_PASSWORD_SELECTION_LOCAL_PASSWORD_LABEL,
                ui::GetChromeOSDeviceName());
  builder->Add("gaiaPasswordSelectionLabel",
               IDS_PASSWORD_SELECTION_GAIA_PASSWORD_LABEL);
}

void PasswordSelectionScreenHandler::Show() {
  ShowInWebUI();
}

void PasswordSelectionScreenHandler::ShowProgress() {
  CallExternalAPI("showProgress");
}

void PasswordSelectionScreenHandler::ShowPasswordChoice() {
  CallExternalAPI("showPasswordChoice");
}

base::WeakPtr<PasswordSelectionScreenView>
PasswordSelectionScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
