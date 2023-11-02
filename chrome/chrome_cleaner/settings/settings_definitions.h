// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SETTINGS_SETTINGS_DEFINITIONS_H_
#define CHROME_CHROME_CLEANER_SETTINGS_SETTINGS_DEFINITIONS_H_

// All executable targets build in this project should contain one
// *_settings_definitions.cc file to ensure that this functions are defined.

namespace chrome_cleaner {

// Indicates what type of Chrome Cleaner binary is running.
enum class TargetBinary {
  kReporter,
  kCleaner,
  kOther,
};

TargetBinary GetTargetBinary();

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SETTINGS_SETTINGS_DEFINITIONS_H_
