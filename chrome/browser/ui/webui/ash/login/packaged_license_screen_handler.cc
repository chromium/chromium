// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/packaged_license_screen_handler.h"

#include "chrome/browser/ash/login/screens/packaged_license_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

PackagedLicenseScreenHandler::PackagedLicenseScreenHandler()
    : BaseScreenHandler(kScreenId) {}

PackagedLicenseScreenHandler::~PackagedLicenseScreenHandler() = default;

void PackagedLicenseScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<PackagedLicenseView> PackagedLicenseScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
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

}  // namespace ash
