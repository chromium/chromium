// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/proto/device_classifier_util.h"

#include <vector>

#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/version.h"
#include "chromeos/ash/services/device_sync/proto/enum_util.h"
#include "components/version_info/version_info.h"

namespace ash {

namespace device_sync {

namespace device_classifier_util {

const cryptauth::DeviceClassifier& GetDeviceClassifier() {
  static const base::NoDestructor<cryptauth::DeviceClassifier> classifier([] {
    cryptauth::DeviceClassifier classifier;

    int32_t major_version;
    int32_t minor_version;
    int32_t bugfix_version;
    base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                                 &bugfix_version);
    classifier.set_device_os_version_code(major_version);
    classifier.set_device_type(DeviceTypeEnumToString(cryptauth::CHROME));

    const std::vector<uint32_t>& version_components =
        version_info::GetVersion().components();
    if (!version_components.empty()) {
      classifier.set_device_software_version_code(version_components[0]);
    }

    classifier.set_device_software_package(
        std::string(version_info::GetProductName()));

    return classifier;
  }());

  return *classifier;
}

}  // namespace device_classifier_util

}  // namespace device_sync

}  // namespace ash
