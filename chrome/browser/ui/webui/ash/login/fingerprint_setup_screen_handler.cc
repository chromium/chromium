// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/fingerprint_setup_screen_handler.h"

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/fingerprint_setup_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

FingerprintSetupScreenHandler::FingerprintSetupScreenHandler()
    : BaseScreenHandler(kScreenId) {}

FingerprintSetupScreenHandler::~FingerprintSetupScreenHandler() = default;

void FingerprintSetupScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("setupFingerprintScreenTitle",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_TITLE);
  builder->Add("setupFingerprintScreenTitleForChild",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_TITLE_CHILD);
  builder->Add("skipFingerprintSetup",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_SKIP);
  builder->Add("fingerprintSetupDone",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_DONE);
  builder->Add("fingerprintSetupAddAnother",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_ADD_ANOTHER);
  builder->Add("enrollmentProgressScreenTitle",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_ENROLLMENT_PROGRESS_TITLE);
  builder->Add("setupFingerprintEnrollmentSuccessTitle",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_ENROLLMENT_SUCCESS_TITLE);
  builder->Add("setupFingerprintEnrollmentSuccessDescription",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_ENROLLMENT_SUCCESS_DESCRIPTION);
  builder->Add(
      "setupFingerprintEnrollmentSuccessDescriptionForChild",
      IDS_OOBE_FINGERPINT_SETUP_SCREEN_ENROLLMENT_SUCCESS_DESCRIPTION_CHILD);
  builder->Add("setupFingerprintScanMoveFinger",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_INSTRUCTION_MOVE_FINGER);
  builder->Add("setupFingerprintScanMoveFingerForChild",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_INSTRUCTION_MOVE_FINGER_CHILD);
  builder->Add("setupFingerprintScanTryAgain",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_INSTRUCTION_TRY_AGAIN);

  auto fp_setup_strings = quick_unlock::GetFingerprintDescriptionStrings(
      quick_unlock::GetFingerprintLocation());
  builder->AddF("setupFingerprintScreenDescription",
                fp_setup_strings.description_id, ui::GetChromeOSDeviceName());
  builder->AddF("setupFingerprintScreenDescriptionForChild",
                fp_setup_strings.description_child_id,
                ui::GetChromeOSDeviceName());
}

void FingerprintSetupScreenHandler::Show() {
  auto* user_manager = user_manager::UserManager::Get();
  base::Value::Dict data;
  data.Set("isChildAccount", user_manager->IsLoggedInAsChildUser());
  ShowInWebUI(std::move(data));
}

void FingerprintSetupScreenHandler::OnEnrollScanDone(
    device::mojom::ScanResult scan_result,
    bool enroll_session_complete,
    int percent_complete) {
  CallExternalAPI("onEnrollScanDone", static_cast<int>(scan_result),
                  enroll_session_complete, percent_complete);
}

void FingerprintSetupScreenHandler::EnableAddAnotherFinger(bool enable) {
  CallExternalAPI("enableAddAnotherFinger", enable);
}

base::WeakPtr<FingerprintSetupScreenView>
FingerprintSetupScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
