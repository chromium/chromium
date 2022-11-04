// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/kiosk_enable_screen_handler.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/ash/login/screens/kiosk_enable_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"

namespace ash {

KioskEnableScreenHandler::KioskEnableScreenHandler()
    : BaseScreenHandler(kScreenId) {}

KioskEnableScreenHandler::~KioskEnableScreenHandler() = default;

void KioskEnableScreenHandler::Show() {
  ShowInWebUI();
}

void KioskEnableScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("kioskEnableWarningText",
               IDS_KIOSK_ENABLE_SCREEN_WARNING);
  builder->Add("kioskEnableWarningDetails",
               IDS_KIOSK_ENABLE_SCREEN_WARNING_DETAILS);
  builder->Add("kioskEnableButton", IDS_KIOSK_ENABLE_SCREEN_ENABLE_BUTTON);
  builder->Add("kioskCancelButton", IDS_CANCEL);
  builder->Add("kioskOKButton", IDS_OK);
  builder->Add("kioskEnableSuccessMsg", IDS_KIOSK_ENABLE_SCREEN_SUCCESS);
  builder->Add("kioskEnableErrorMsg", IDS_KIOSK_ENABLE_SCREEN_ERROR);
}

void KioskEnableScreenHandler::ShowKioskEnabled(bool success) {
  CallExternalAPI("onCompleted", success);
}

}  // namespace ash
