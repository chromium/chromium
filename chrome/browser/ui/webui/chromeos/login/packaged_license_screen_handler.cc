// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/packaged_license_screen_handler.h"

#include "chrome/browser/ash/login/screens/packaged_license_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId PackagedLicenseView::kScreenId;

PackagedLicenseScreenHandler::PackagedLicenseScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.PackagedLicenseScreen.userActed");
}

PackagedLicenseScreenHandler::~PackagedLicenseScreenHandler() {}

void PackagedLicenseScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void PackagedLicenseScreenHandler::Hide() {}

void PackagedLicenseScreenHandler::Bind(PackagedLicenseScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void PackagedLicenseScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void PackagedLicenseScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("oobePackagedLicenseTitle", IDS_OOBE_PACKAGED_LICENSE_TITLE);
  builder->Add("oobePackagedLicenseSubtitleP1",
               IDS_OOBE_PACKGED_LICENSE_SUBTITLE_P1);
  builder->Add("oobePackagedLicenseSubtitleP2",
               IDS_OOBE_PACKGED_LICENSE_SUBTITLE_P2);
  builder->Add("oobePackagedLicenseEnroll",
               IDS_OOBE_PACKAGED_LICENSE_ENROLL_BUTTON_LABEL);
  builder->Add("oobePackagedLicenseDontEnroll",
               IDS_OOBE_PACKAGED_LICENSE_DONT_ENROLL_BUTTON_LABEL);
}

void PackagedLicenseScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

}  // namespace chromeos
