// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/fingerprint_setup_screen_handler.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/screens/fingerprint_setup_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/system_connector.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// The max number of fingerprints that can be stored.
constexpr int kMaxAllowedFingerprints = 3;

// Determines what the newly added fingerprint's name should be.
std::string GetDefaultFingerprintName(int enrolled_finger_count) {
  DCHECK(enrolled_finger_count < kMaxAllowedFingerprints);
  switch (enrolled_finger_count) {
    case 0:
      return l10n_util::GetStringUTF8(
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_NEW_FINGERPRINT_DEFAULT_NAME_1);
    case 1:
      return l10n_util::GetStringUTF8(
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_NEW_FINGERPRINT_DEFAULT_NAME_2);
    case 2:
      return l10n_util::GetStringUTF8(
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_NEW_FINGERPRINT_DEFAULT_NAME_3);
    default:
      NOTREACHED();
  }
  return std::string();
}

}  // namespace

namespace chromeos {

constexpr StaticOobeScreenId FingerprintSetupScreenView::kScreenId;

FingerprintSetupScreenHandler::FingerprintSetupScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.FingerprintSetupScreen.userActed");

  content::GetSystemConnector()->Connect(
      device::mojom::kServiceName, fp_service_.BindNewPipeAndPassReceiver());
  fp_service_->AddFingerprintObserver(receiver_.BindNewPipeAndPassRemote());
}

FingerprintSetupScreenHandler::~FingerprintSetupScreenHandler() = default;

void FingerprintSetupScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("setupFingerprintScreenTitle",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_TITLE);
  builder->Add("skipFingerprintSetup",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_SKIP);
  builder->Add("fingerprintSetupLater",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_LATER);
  builder->Add("fingerprintSetupNext",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_NEXT);
  builder->Add("fingerprintSetupDone",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_DONE);
  builder->Add("fingerprintSetupAddAnother",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_BUTTON_ADD_ANOTHER);
  builder->Add("placeFingerScreenTitle",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_LOCATION_TITLE);
  builder->Add("enrollmentProgressScreenTitle",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_ENROLLMENT_PROGRESS_TITLE);
  builder->Add("setupFingerprintEnrollmentSuccessTitle",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_ENROLLMENT_SUCCESS_TITLE);
  builder->Add("setupFingerprintEnrollmentSuccessDescription",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_ENROLLMENT_SUCCESS_DESCRIPTION);
  builder->Add("setupFingerprintScanMoveFinger",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_INSTRUCTION_MOVE_FINGER);
  builder->Add("setupFingerprintScanTryAgain",
               IDS_OOBE_FINGERPINT_SETUP_SCREEN_INSTRUCTION_TRY_AGAIN);
  int description_id;
  switch (quick_unlock::GetFingerprintLocation()) {
    case quick_unlock::FingerprintLocation::TABLET_POWER_BUTTON:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_POWER_BUTTON_DESCRIPTION;
      break;
    case quick_unlock::FingerprintLocation::KEYBOARD_BOTTOM_RIGHT:
    case quick_unlock::FingerprintLocation::KEYBOARD_TOP_RIGHT:
      description_id =
          IDS_OOBE_FINGERPINT_SETUP_SCREEN_SENSOR_GENERAL_DESCRIPTION;
      break;
  }
  builder->Add("setupFingerprintScreenDescription", description_id);
}

void FingerprintSetupScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
  web_ui()->RegisterMessageCallback(
      "startEnroll",
      base::BindRepeating(&FingerprintSetupScreenHandler::HandleStartEnroll,
                          base::Unretained(this)));
}

void FingerprintSetupScreenHandler::Bind(FingerprintSetupScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void FingerprintSetupScreenHandler::Show() {
  ShowScreen(kScreenId);
}

void FingerprintSetupScreenHandler::Hide() {
  // Clean up existing fingerprint enroll session.
  if (enroll_session_started_) {
    fp_service_->CancelCurrentEnrollSession(base::BindOnce(
        &FingerprintSetupScreenHandler::OnCancelCurrentEnrollSession,
        weak_ptr_factory_.GetWeakPtr()));
  }
}

void FingerprintSetupScreenHandler::Initialize() {}

void FingerprintSetupScreenHandler::OnRestarted() {
  VLOG(1) << "Fingerprint session restarted.";
}

void FingerprintSetupScreenHandler::OnEnrollScanDone(
    device::mojom::ScanResult scan_result,
    bool enroll_session_complete,
    int percent_complete) {
  VLOG(1) << "Receive fingerprint enroll scan result. scan_result="
          << scan_result
          << ", enroll_session_complete=" << enroll_session_complete
          << ", percent_complete=" << percent_complete;
  CallJS("login.FingerprintSetupScreen.onEnrollScanDone",
         static_cast<int>(scan_result), enroll_session_complete,
         percent_complete);

  if (enroll_session_complete) {
    enroll_session_started_ = false;

    ++enrolled_finger_count_;
    CallJS("login.FingerprintSetupScreen.enableAddAnotherFinger",
           enrolled_finger_count_ < kMaxAllowedFingerprints);

    // Update the number of registered fingers, it's fine to override because
    // this is the first time user log in and have no finger registered.
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetInteger(
        prefs::kQuickUnlockFingerprintRecord, enrolled_finger_count_);
  }
}

void FingerprintSetupScreenHandler::OnAuthScanDone(
    device::mojom::ScanResult scan_result,
    const base::flat_map<std::string, std::vector<std::string>>& matches) {}

void FingerprintSetupScreenHandler::OnSessionFailed() {
  // TODO(xiaoyinh): Add more user visible information when available.
  LOG(ERROR) << "Fingerprint session failed.";
}

void FingerprintSetupScreenHandler::HandleStartEnroll(
    const base::ListValue* args) {
  DCHECK(enrolled_finger_count_ < kMaxAllowedFingerprints);

  enroll_session_started_ = true;
  fp_service_->StartEnrollSession(
      ProfileHelper::Get()->GetUserIdHashFromProfile(
          ProfileManager::GetActiveUserProfile()),
      GetDefaultFingerprintName(enrolled_finger_count_));
}

void FingerprintSetupScreenHandler::OnCancelCurrentEnrollSession(bool success) {
  if (!success)
    LOG(ERROR) << "Failed to cancel current fingerprint enroll session.";
}

}  // namespace chromeos
