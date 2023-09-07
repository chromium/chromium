// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/chromebox_for_meetings/features.h"

namespace ash::cfm::features {

BASE_FEATURE(kCloudLogger,
             "MeetDevicesCloudLogger",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMojoServices,
             "MeetDevicesMojoServices",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kXuControls,
             "MeetDevicesXuControls",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace ash::cfm::features
