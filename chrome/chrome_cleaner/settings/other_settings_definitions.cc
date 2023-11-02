// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides the target binary to be used by executables other than the reporter
// and cleaner.

#include "chrome/chrome_cleaner/settings/settings_definitions.h"

namespace chrome_cleaner {

TargetBinary GetTargetBinary() {
  return TargetBinary::kOther;
}

}  // namespace chrome_cleaner
