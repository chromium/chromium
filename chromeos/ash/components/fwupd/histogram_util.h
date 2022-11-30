// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FWUPD_HISTOGRAM_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_FWUPD_HISTOGRAM_UTIL_H_

#include <cstdint>
#include <string>

namespace ash {
namespace firmware_update {
namespace metrics {

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml: FirmwareUpdateInstallResult.
enum class FirmwareUpdateInstallResult {
  kSuccess = 0,
  kInstallFailed = 1,
  kFailedToCreateUpdateDirectory = 2,
  kInvalidDestinationFile = 3,
  kInvalidFileDescriptor = 4,
  kFailedToDownloadToFile = 5,
  kMaxValue = kFailedToDownloadToFile,
};

void EmitDeviceCount(int num_devices, bool is_startup);

void EmitUpdateCount(int num_updates,
                     int num_critical_updates,
                     bool is_startup);

void EmitInstallResult(FirmwareUpdateInstallResult result);

std::string GetSourceStr(bool is_startup);

}  // namespace metrics
}  // namespace firmware_update
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_FWUPD_HISTOGRAM_UTIL_H_
