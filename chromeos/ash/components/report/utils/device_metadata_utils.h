// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_DEVICE_METADATA_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_DEVICE_METADATA_UTILS_H_

#include <string>

#include "chromeos/ash/components/report/proto/fresnel_service.pb.h"

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash::report::utils {

// Return the release Chrome Channel enum defined in fresnel service proto.
ash::report::Channel GetChromeChannel(version_info::Channel channel);

// Retrieve the major Chrome Milestone of the ChromeOS device as a string.
std::string GetChromeMilestone();

// Retrieve full hardware class from MachineStatistics as a string.
std::string GetFullHardwareClass();

}  // namespace ash::report::utils

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_DEVICE_METADATA_UTILS_H_
