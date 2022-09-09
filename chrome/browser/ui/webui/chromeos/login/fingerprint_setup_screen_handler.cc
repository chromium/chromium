// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/fingerprint_setup_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

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

  int description_id, aria_label_id, description_id_for_child;
  bool aria_label_includes_device = false;
  switch (quick_unlock::GetFingerprintLocation()) {
    case quick_unlock::FingerprintLocation::TABLET_POWER_BUTTON:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_POWER_BUTTON_DESCRIPTION;
      description_id_for_child =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_POWER_BUTTON_DESCRIPTION_CHILD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_POWER_BUTTON_ARIA_LABEL;
      break;
    case quick_unlock::FingerprintLocation::KEYBOARD_BOTTOM_LEFT:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION;
      description_id_for_child =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_BOTTOM_LEFT_ARIA_LABEL;
      break;
    case quick_unlock::FingerprintLocation::KEYBOARD_BOTTOM_RIGHT:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION;
      description_id_for_child =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_BOTTOM_RIGHT_ARIA_LABEL;
      break;
    case quick_unlock::FingerprintLocation::KEYBOARD_TOP_RIGHT:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION;
      description_id_for_child =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_TOP_RIGHT_ARIA_LABEL;
      break;
    case quick_unlock::FingerprintLocation::RIGHT_SIDE:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION;
      description_id_for_child =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_RIGHT_SIDE_ARIA_LABEL;
      aria_label_includes_device = true;
      break;
    case quick_unlock::FingerprintLocation::LEFT_SIDE:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION;
      description_id_for_child =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD;
      aria_label_id =
          IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_LEFT_SIDE_ARIA_LABEL;
      aria_label_includes_device = true;
      break;
    case quick_unlock::FingerprintLocation::LEFT_OF_POWER_BUTTON_TOP_RIGHT:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_LEFT_OF_POWER_BUTTON_TOP_RIGHT;
      description_id_for_child =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_LEFT_OF_POWER_BUTTON_TOP_RIGHT_CHILD;
      break;
    case quick_unlock::FingerprintLocation::UNKNOWN:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION;
      description_id_for_child =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD;
      aria_label_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION;
      aria_label_includes_device = true;
      break;
  }
  builder->AddF("setupFingerprintScreenDescription", description_id,
                ui::GetChromeOSDeviceName());
  builder->AddF("setupFingerprintScreenDescriptionForChild",
                description_id_for_child, ui::GetChromeOSDeviceName());

  if (ash::quick_unlock::IsLeftOfPowerButtonTopRightFingerprint())
    return;

  if (aria_label_includes_device) {
    builder->AddF("setupFingerprintScreenAriaLabel", aria_label_id,
                  ui::GetChromeOSDeviceName());
  } else {
    builder->Add("setupFingerprintScreenAriaLabel", aria_label_id);
  }
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

}  // namespace chromeos
