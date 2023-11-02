// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/multidevice_setup_screen_handler.h"

#include "ash/constants/ash_features.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/multidevice_setup_screen.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_localized_strings_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

MultiDeviceSetupScreenHandler::MultiDeviceSetupScreenHandler()
    : BaseScreenHandler(kScreenId) {}

MultiDeviceSetupScreenHandler::~MultiDeviceSetupScreenHandler() = default;

void MultiDeviceSetupScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  multidevice_setup::AddLocalizedValuesToBuilder(builder);
}

void MultiDeviceSetupScreenHandler::Show() {
  ShowInWebUI();
  FireWebUIListenerWhenAllowed("multidevice_setup.initializeSetupFlow");
}

void MultiDeviceSetupScreenHandler::GetAdditionalParameters(
    base::Value::Dict* dict) {
  dict->Set("wifiSyncEnabled",
            base::Value(ash::features::IsWifiSyncAndroidEnabled()));
}

}  // namespace chromeos
