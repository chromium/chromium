// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/enterprise_switches.h"

#include "build/build_config.h"

namespace switches {

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
// Enables the Chrome Browser Cloud Management integration on Chromium builds.
// CBCM is always enabled in branded builds.
const char kEnableChromeBrowserCloudManagement[] =
    "enable-chrome-browser-cloud-management";
#endif

}  // namespace switches
