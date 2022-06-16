// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A file containing extended-variations-safe-mode-related constants, etc.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SAFE_MODE_CONSTANTS_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SAFE_MODE_CONSTANTS_H_

#include "base/files/file_path.h"

namespace variations {

// The name of the beacon file, which is relative to the user data directory
// and used to store the CleanExitBeacon value and the variations crash streak.
extern const base::FilePath::CharType kCleanExitBeaconFilename[];

// Trial and group names for the extended variations safe mode experiment.
extern const char kExtendedSafeModeTrial[];
extern const char kControlGroup[];
extern const char kDefaultGroup[];
extern const char kEnabledGroup[];

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SAFE_MODE_CONSTANTS_H_
