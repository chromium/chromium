// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_FEATURES_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_FEATURES_H_

#include "base/feature_list.h"

namespace data_controls {

// Controls enabling Data Controls for all desktop browser platforms (Windows,
// Mac, Linux, CrOS). Policies controlling cross-platform Data Controls will be
// ignored if this feature is disabled.
BASE_DECLARE_FEATURE(kEnableDesktopDataControls);

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_FEATURES_H_
