// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SETTINGS_SETTINGS_TYPES_H_
#define CHROME_CHROME_CLEANER_SETTINGS_SETTINGS_TYPES_H_

namespace chrome_cleaner {

// Values to be passed to kSandboxType and that will be used to determine the
// type of sandbox that should be created on a certain process.
enum class SandboxType {
  // Signal no sandbox.
  kNonSandboxed = 0,
  kTest,
  kEngine,
  kParser,
  kZipArchiver,
  kNumValues,
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SETTINGS_SETTINGS_TYPES_H_
