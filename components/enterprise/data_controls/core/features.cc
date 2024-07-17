// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/features.h"

namespace data_controls {

BASE_FEATURE(kEnableDesktopDataControls,
             "EnableDesktopDataControls",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableScreenshotProtection,
             "EnableScreenshotProtection",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace data_controls
