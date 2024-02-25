// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/installed_version_updater_chromeos.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"

namespace {

// The reason of the rollback used in the UpgradeDetector.RollbackReason
// histogram.
enum class RollbackReason {
  kToMoreStableChannel = 0,
  kEnterpriseRollback = 1,
  kMaxValue = kEnterpriseRollback,
};

}  // namespace

InstalledVersionUpdater::InstalledVersionUpdater(BuildState* build_state)
    : build_state_(build_state) {
  ash::UpdateEngineClient::Get()->AddObserver(this);
}

InstalledVersionUpdater::~InstalledVersionUpdater() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ash::UpdateEngineClient::Get()->RemoveObserver(this);
}

void InstalledVersionUpdater::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If status changes to `IDLE`, there is no currently available update.
  if (status.current_operation() == update_engine::Operation::IDLE) {
    build_state_->SetUpdate(BuildState::UpdateType::kNone, base::Version(),
                            std::nullopt);
    return;
  }

  if (status.current_operation() !=
      update_engine::Operation::UPDATED_NEED_REBOOT) {
    return;
  }

  BuildState::UpdateType update_type = BuildState::UpdateType::kNormalUpdate;

  if (status.will_powerwash_after_reboot()) {
    // Powerwash will be required, this can be triggered by an enterprise
    // rollback or by the user switching to a more stable channel. Determine
    // what kind of notification to show based on the enterprise rollback flag.

    if (status.is_enterprise_rollback()) {
      update_type = BuildState::UpdateType::kEnterpriseRollback;

      base::UmaHistogramEnumeration("UpgradeDetector.RollbackReason",
                                    RollbackReason::kEnterpriseRollback);

      LOG(WARNING) << "Device is rolling back, will require powerwash. Reason:"
                   << " Enterprise rollback.";

    } else {
      // Powerwash must have been triggered by channel change.
      update_type = BuildState::UpdateType::kChannelSwitchRollback;

      base::UmaHistogramEnumeration("UpgradeDetector.RollbackReason",
                                    RollbackReason::kToMoreStableChannel);

      LOG(WARNING) << "Device is rolling back, will require powerwash. Reason:"
                   << " Channel switch.";
    }
  }
  build_state_->SetUpdate(update_type, base::Version(status.new_version()),
                          std::nullopt);
}
