// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/report/utils/device_metadata_utils.h"

#include <string_view>

#include "base/logging.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"

namespace ash::report::utils {

namespace {

// Default value for devices that are missing the hardware class.
const char kHardwareClassKeyNotFound[] = "HARDWARE_CLASS_KEY_NOT_FOUND";

}  // namespace

ash::report::Channel GetChromeChannel(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::CANARY:
      return Channel::CHANNEL_CANARY;
    case version_info::Channel::DEV:
      return Channel::CHANNEL_DEV;
    case version_info::Channel::BETA:
      return Channel::CHANNEL_BETA;
    case version_info::Channel::STABLE:
      return Channel::CHANNEL_STABLE;
    case version_info::Channel::UNKNOWN:
    default:
      return Channel::CHANNEL_UNKNOWN;
  }
}

std::string GetChromeMilestone() {
  return version_info::GetMajorVersionNumber();
}

std::string GetFullHardwareClass() {
  const std::optional<std::string_view> full_hardware_class =
      system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          system::kHardwareClassKey);

  if (!full_hardware_class.has_value()) {
    LOG(ERROR) << "Hardware class failed to be retrieved - returning value "
               << kHardwareClassKeyNotFound;
    return std::string(kHardwareClassKeyNotFound);
  }

  return std::string(full_hardware_class.value());
}

}  // namespace ash::report::utils
