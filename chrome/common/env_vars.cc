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

// CHROME_CRASHED exists if a previous instance of chrome has crashed. This
// triggers the 'restart chrome' dialog. CHROME_RESTART contains the strings
// that are needed to show the dialog.
const char kShowRestart[] = "CHROME_CRASHED";
const char kRestartInfo[] = "CHROME_RESTART";

// The strings RIGHT_TO_LEFT and LEFT_TO_RIGHT indicate the locale direction.
// For example, for Hebrew and Arabic locales, we use RIGHT_TO_LEFT so that the
// dialog is displayed using the right orientation.
const char kRtlLocale[] = "RIGHT_TO_LEFT";
const char kLtrLocale[] = "LEFT_TO_RIGHT";

}  // namespace env_vars
