// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/drive_disclaimer_controller.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace drive_picker {

namespace {
constexpr char kApplicationId[] = "chrome_desktop_disclaimer";
}  // namespace

DriveDisclaimerController::DriveDisclaimerController(
    std::unique_ptr<contextual_search::FpopService> fpop_service)
    : fpop_service_(std::move(fpop_service)) {
  CHECK(fpop_service_);
}

DriveDisclaimerController::~DriveDisclaimerController() = default;

void DriveDisclaimerController::CheckDisclaimerStatusAsync(
    base::OnceCallback<void(DisclaimerStatus status)> completion_callback) {
  footprints::oneplatform::GetFacsRequest request;
  request.add_setting(
      contextual_search::kContextualSearchDriveDisclaimerAccepted);
  request.mutable_header()->set_application_id(kApplicationId);

  fpop_service_->GetFacs(
      request, base::BindOnce(&DriveDisclaimerController::OnGetFacsResponse,
                              weak_factory_.GetWeakPtr(),
                              std::move(completion_callback)));
}

void DriveDisclaimerController::OnGetFacsResponse(
    base::OnceCallback<void(DisclaimerStatus status)> completion_callback,
    bool success,
    const footprints::oneplatform::GetFacsResponse& response) {
  if (success) {
    for (const auto& facs_setting : response.facs_setting()) {
      if (facs_setting.setting() ==
          contextual_search::kContextualSearchDriveDisclaimerAccepted) {
        if (facs_setting.recording_setting_info()
                .user_setting_restricted_reason_size() > 0) {
          std::move(completion_callback).Run(DisclaimerStatus::kRestricted);
          return;
        }
        if (facs_setting.data_recording_enabled()) {
          std::move(completion_callback).Run(DisclaimerStatus::kAccepted);
          return;
        }
      }
    }
  }

  std::move(completion_callback).Run(DisclaimerStatus::kNotAccepted);
}

}  // namespace drive_picker
