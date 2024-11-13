// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/env_vars.h"

namespace env_vars {

// We call running in unattended mode (for automated testing) "headless".
// This mode can be enabled using this variable or by the kNoErrorDialogs
// switch.
const char kHeadless[] = "CHROME_HEADLESS";

// The name of the log file.
const char kLogFileName[] = "CHROME_LOG_FILE";

// Flag indicating if metro viewer is connected to browser instance.
// As of now there is only one metro viewer instance per browser.
const char kMetroConnected[] = "CHROME_METRO_CONNECTED";

// The name of the session log directory when logged in to ChromeOS.
const char kSessionLogDir[] = "CHROMEOS_SESSION_LOG_DIR";

}  // namespace env_vars
