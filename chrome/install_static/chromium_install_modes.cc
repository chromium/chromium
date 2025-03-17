// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific constants and install modes for Chromium.

#include "chrome/install_static/chromium_install_modes.h"

#include <stdlib.h>

#include "chrome/install_static/install_modes.h"

namespace install_static {

const wchar_t kCompanyPathName[] = L"";

const wchar_t kProductPathName[] = L"Chromium";

const size_t kProductPathNameLength = _countof(kProductPathName) - 1;

const char kSafeBrowsingName[] = "chromium";

}  // namespace install_static
