// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/kiosk_enable_screen_handler.h"

#include <string>

#include "base/bind.h"
#include "chrome/browser/ash/login/screens/kiosk_enable_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"

namespace chromeos {

constexpr StaticOobeScreenId KioskEnableScreenView::kScreenId;

KioskEnableScreenHandler::KioskEnableScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.KioskEnableScreen.userActed");
}

KioskEnableScreenHandler::~KioskEnableScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void KioskEnableScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void KioskEnableScreenHandler::SetScreen(KioskEnableScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
  screen_ = screen;
  if (page_is_ready() && screen_)
    Initialize();
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

void KioskEnableScreenHandler::Initialize() {
  if (!page_is_ready() || !screen_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void KioskEnableScreenHandler::ShowKioskEnabled(bool success) {
  CallJS("login.KioskEnableScreen.onCompleted", success);
}

}  // namespace chromeos
