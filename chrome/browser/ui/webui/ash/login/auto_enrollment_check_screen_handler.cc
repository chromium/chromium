// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/auto_enrollment_check_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

AutoEnrollmentCheckScreenHandler::AutoEnrollmentCheckScreenHandler()
    : BaseScreenHandler(kScreenId) {}

AutoEnrollmentCheckScreenHandler::~AutoEnrollmentCheckScreenHandler() = default;

void AutoEnrollmentCheckScreenHandler::Show() {
  ShowInWebUI();
}

void AutoEnrollmentCheckScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("autoEnrollmentCheckMessage",
               IDS_AUTO_ENROLLMENT_CHECK_SCREEN_MESSAGE);
  builder->Add("gettingDeviceReadyTitle", IDS_GETTING_DEVICE_READY);
}

base::WeakPtr<AutoEnrollmentCheckScreenView>
AutoEnrollmentCheckScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
