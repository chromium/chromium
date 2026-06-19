// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/drive_disclaimer_controller.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "components/omnibox/common/omnibox_features.h"

namespace drive_picker {

namespace {
constexpr char kApplicationId[] = "chrome_desktop_disclaimer";
}  // namespace

// static
std::string DriveDisclaimerController::DisclaimerStatusToString(
    DisclaimerStatus status) {
  switch (status) {
    case DisclaimerStatus::kAccepted:
      return "Accepted";
    case DisclaimerStatus::kNotAccepted:
      return "NotAccepted";
    case DisclaimerStatus::kRestricted:
      return "Restricted";
  }
}

DriveDisclaimerController::DriveDisclaimerController(
    std::unique_ptr<contextual_search::FpopService> fpop_service)
    : fpop_service_(std::move(fpop_service)) {
  CHECK(fpop_service_);
}

DriveDisclaimerController::~DriveDisclaimerController() = default;

void DriveDisclaimerController::CheckDisclaimerStatusAsync(
    base::OnceCallback<void(DisclaimerStatus status)> completion_callback) {
  DVLOG(1) << "DriveDisclaimerController::CheckDisclaimerStatusAsync: Checking "
              "FACS status for PersonalContextSearchUsingWorkspace.";
  // This flag is used for testing purposes only to force the disclaimer to be
  // accepted.
  if (base::FeatureList::IsEnabled(omnibox::kForceDriveDisclaimerAccepted)) {
    DVLOG(1) << "DriveDisclaimerController::CheckDisclaimerStatusAsync: "
                "kForceDriveDisclaimerAccepted is enabled, forcing kAccepted.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_callback),
                                  DisclaimerStatus::kAccepted));
    return;
  }

  footprints::oneplatform::GetFacsRequest request;
  request.add_setting(contextual_search::kPersonalContextSearchUsingWorkspace);
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
  DisclaimerStatus final_status = DisclaimerStatus::kNotAccepted;
  if (success) {
    DVLOG(1) << "DriveDisclaimerController::OnGetFacsResponse: FACS request "
                "succeeded. settings count = "
             << response.facs_setting_size();
    for (const auto& facs_setting : response.facs_setting()) {
      if (facs_setting.setting() ==
          contextual_search::kPersonalContextSearchUsingWorkspace) {
        bool restricted = facs_setting.recording_setting_info()
                              .user_setting_restricted_reason_size() > 0;
        bool enabled = facs_setting.data_recording_enabled();
        DVLOG(1) << "DriveDisclaimerController::OnGetFacsResponse: Found "
                    "workspace personal search setting. "
                 << "data_recording_enabled = " << enabled
                 << ", restricted = " << restricted;
        if (restricted) {
          final_status = DisclaimerStatus::kRestricted;
          break;
        }
        if (enabled) {
          final_status = DisclaimerStatus::kAccepted;
          break;
        }
      }
    }
  } else {
    LOG(WARNING)
        << "DriveDisclaimerController::OnGetFacsResponse: FACS request failed.";
  }

  DVLOG(1) << "DriveDisclaimerController::OnGetFacsResponse: returning status "
           << DisclaimerStatusToString(final_status);
  std::move(completion_callback).Run(final_status);
}

}  // namespace drive_picker
