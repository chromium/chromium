// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/install_attributes_error_screen_handler.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/user_manager.h"

namespace ash {

InstallAttributesErrorScreenHandler::InstallAttributesErrorScreenHandler()
    : BaseScreenHandler(kScreenId) {}

InstallAttributesErrorScreenHandler::~InstallAttributesErrorScreenHandler() =
    default;

void InstallAttributesErrorScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("errorInstallAttributesFailureTitle",
               IDS_DEVICE_ERROR_INSTALL_ATTRIBUTES_FAILURE_TITLE);
  builder->Add("errorInstallAttributesFailureReset",
               IDS_DEVICE_ERROR_INSTALL_ATTRIBUTES_FAILURE_RESET);
  builder->Add("errorInstallAttributesFailureRestart",
               IDS_DEVICE_ERROR_INSTALL_ATTRIBUTES_FAILURE_RESTART);
  builder->Add("resetButtonRestart", IDS_RELAUNCH_BUTTON);
  builder->Add("resetButtonPowerwash", IDS_RESET_SCREEN_POWERWASH);
}

void InstallAttributesErrorScreenHandler::Show() {
  bool restart_required = user_manager::UserManager::Get()->IsUserLoggedIn() ||
                          !base::CommandLine::ForCurrentProcess()->HasSwitch(
                              switches::kFirstExecAfterBoot);
  base::Value::Dict data;
  data.Set("restartRequired", restart_required);
  ShowInWebUI(std::move(data));
}

base::WeakPtr<InstallAttributesErrorView>
InstallAttributesErrorScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
