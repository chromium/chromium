// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_FEATURES_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_FEATURES_H_

#include "base/feature_list.h"

namespace data_controls {

// Controls enabling screenshot blocking Data Controls rules for supported
// desktop browser platforms (Windows, Mac).
//
// Use `kEnableDesktopDataControls` to gate the implementation ofother rule
// types.
BASE_DECLARE_FEATURE(kEnableScreenshotProtection);

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_FEATURES_H_
