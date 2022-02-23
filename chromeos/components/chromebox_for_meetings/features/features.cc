// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/chromebox_for_meetings/features/features.h"

namespace chromeos {
namespace cfm {
namespace features {

const base::Feature kCloudLogger{"MeetDevicesCloudLogger",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMojoServices{"MeetDevicesMojoServices",
                                  base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace features
}  // namespace cfm
}  // namespace chromeos
