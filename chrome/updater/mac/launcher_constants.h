// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_LAUNCHER_CONSTANTS_H_
#define CHROME_UPDATER_MAC_LAUNCHER_CONSTANTS_H_

#include <stdbool.h>

// The path to the executable varies between test and production builds, and by
// vendor. Refer to //docs/updater/design_doc.md#testing.
extern const char kBundlePath[];
extern const char kExecutablePath[];
extern const char kExecutableName[];

// Test builds skip the signing checks.
extern const bool kCheckSigning;

#endif  // CHROME_UPDATER_MAC_LAUNCHER_CONSTANTS_H_
