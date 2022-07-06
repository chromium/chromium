// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SAFE_MODE_CONSTANTS_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SAFE_MODE_CONSTANTS_H_

#include "base/files/file_path.h"

namespace variations {

// The name of the beacon file, which is relative to the user data directory
// and used to store the CleanExitBeacon value and the variations crash streak.
//
// TODO(crbug/1341854): Find a home for this constant under components/metrics.
extern const base::FilePath::CharType kCleanExitBeaconFilename[];

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SAFE_MODE_CONSTANTS_H_
