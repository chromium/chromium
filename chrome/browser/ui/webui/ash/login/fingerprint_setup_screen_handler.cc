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

struct SensorLocationToString {
  uint32_t description_id = 0;
  uint32_t description_child_id = 0;
  uint32_t aria_label_id = 0;
  bool aria_label_includes_device = false;
};

constexpr auto kLocationToStringMap = base::MakeFixedFlatMap<
    quick_unlock::FingerprintLocation,
    SensorLocationToString>(
    {{quick_unlock::FingerprintLocation::TABLET_POWER_BUTTON,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_POWER_BUTTON_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_POWER_BUTTON_DESCRIPTION_CHILD,
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_POWER_BUTTON_ARIA_LABEL,
       false /*aria_label_includes_device*/}},
     {quick_unlock::FingerprintLocation::KEYBOARD_BOTTOM_LEFT,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD,
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_BOTTOM_LEFT_ARIA_LABEL,
       false /*aria_label_includes_device*/}},
     {quick_unlock::FingerprintLocation::KEYBOARD_BOTTOM_RIGHT,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD,
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_BOTTOM_RIGHT_ARIA_LABEL,
       false /*aria_label_includes_device*/}},
     {quick_unlock::FingerprintLocation::KEYBOARD_TOP_RIGHT,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD,
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_KEYBOARD_TOP_RIGHT_ARIA_LABEL,
       false /*aria_label_includes_device*/}},
     {quick_unlock::FingerprintLocation::RIGHT_SIDE,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD,
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_RIGHT_SIDE_ARIA_LABEL,
       true /*aria_label_includes_device*/}},
     {quick_unlock::FingerprintLocation::LEFT_SIDE,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD,
       IDS_SETTINGS_ADD_FINGERPRINT_DIALOG_INSTRUCTION_LOCATE_SCANNER_LEFT_SIDE_ARIA_LABEL,
       true /*aria_label_includes_device*/}},
     {quick_unlock::FingerprintLocation::LEFT_OF_POWER_BUTTON_TOP_RIGHT,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_LEFT_OF_POWER_BUTTON_TOP_RIGHT,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_LEFT_OF_POWER_BUTTON_TOP_RIGHT_CHILD}},
     {quick_unlock::FingerprintLocation::UNKNOWN,
      {IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION_CHILD,
       IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION,
       true /*aria_label_includes_device*/}}});

SensorLocationToString GetSensorInfo() {
  auto* location_string_it =
      kLocationToStringMap.find(quick_unlock::GetFingerprintLocation());
  CHECK(location_string_it != kLocationToStringMap.end());
  return location_string_it->second;
}

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

  auto* location_string_it =
      kLocationToStringMap.find(quick_unlock::GetFingerprintLocation());
  CHECK(location_string_it != kLocationToStringMap.end());
  auto sensor_info = GetSensorInfo();

  builder->AddF("setupFingerprintScreenDescription", sensor_info.description_id,
                ui::GetChromeOSDeviceName());
  builder->AddF("setupFingerprintScreenDescriptionForChild",
                sensor_info.description_child_id, ui::GetChromeOSDeviceName());

  if (sensor_info.aria_label_id != 0) {
    if (sensor_info.aria_label_includes_device) {
      builder->AddF("setupFingerprintScreenAriaLabel",
                    sensor_info.aria_label_id, ui::GetChromeOSDeviceName());
    } else {
      builder->Add("setupFingerprintScreenAriaLabel",
                   sensor_info.aria_label_id);
    }
  }
}

void FingerprintSetupScreenHandler::Show() {
  auto* user_manager = user_manager::UserManager::Get();
  base::Value::Dict data;
  data.Set("isChildAccount", user_manager->IsLoggedInAsChildUser());
  data.Set("hasAriaLabel", GetSensorInfo().aria_label_id != 0);
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

}  // namespace ash
