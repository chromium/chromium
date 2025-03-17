// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific constants and install modes for Google Chrome for Testing.

#include "chrome/install_static/google_chrome_for_testing_install_modes.h"

#include <stdlib.h>

#include "chrome/install_static/install_modes.h"

namespace install_static {

const wchar_t kCompanyPathName[] = L"Google";

const wchar_t kProductPathName[] = L"Chrome for Testing";

const size_t kProductPathNameLength = _countof(kProductPathName) - 1;

const char kSafeBrowsingName[] = "googlechromefortesting";

}  // namespace install_static
