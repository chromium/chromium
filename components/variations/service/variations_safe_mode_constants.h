// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A file containing extended-variations-safe-mode-related constants, etc.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SAFE_MODE_CONSTANTS_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SAFE_MODE_CONSTANTS_H_

#include "base/files/file_path.h"

namespace variations {

// The name of the file, which is relative to the user data directory, used by
// the Extended Variations Safe Mode experiment's SignalAndWriteViaFileUtilGroup
// to store the stability beacon and the variations crash streak.
extern const base::FilePath::CharType kVariationsFilename[];

// Trial and group names for the extended variations safe mode experiment.
extern const char kExtendedSafeModeTrial[];
extern const char kControlGroup[];
extern const char kDefaultGroup[];
extern const char kEnabledGroup[];

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SAFE_MODE_CONSTANTS_H_
