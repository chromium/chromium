// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/constants/quarantine_constants.h"

namespace chrome_cleaner {

// The quarantine folder name under Chrome Cleanup folder.
constexpr wchar_t kQuarantineFolder[] = L"Quarantine";

// Fixed zip archive password for quarantine.
constexpr char kQuarantinePassword[] = "chrome_cleanup";

// The size limit of source file is 1GB.
constexpr int64_t kQuarantineSourceSizeLimit = 1024 * 1024 * 1024;

}  // namespace chrome_cleaner
