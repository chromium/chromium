// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_TEST_APP_CONSTANTS_H_
#define CHROME_UPDATER_TEST_TEST_APP_CONSTANTS_H_

#include "build/build_config.h"

namespace updater {

// Installs the updater.
extern const char kInstallUpdaterSwitch[];

// On Mac: Tries to register with the updater. Installs the updater if needed.
extern const char kRegisterUpdaterSwitch[];

// Registers the test app to the updater through IPC.
extern const char kRegisterToUpdaterSwitch[];

// Initiates a foreground update through IPC.
extern const char kForegroundUpdateSwitch[];

// The application ID of the test app.
extern const char kTestAppId[];

// Update process state machine.
enum class UpdateStatus {
  INIT = 0,
  CHECKING = 1,
  NEED_PERMISSION_TO_UPDATE = 2,
  UPDATING = 3,
  NEARLY_UPDATED = 4,
  UPDATED = 5,
  FAILED = 6,
  FAILED_OFFLINE = 7,
  FAILED_CONNECTION_TYPE_DISALLOWED = 8,
  DISABLED = 9,
  DISABLED_BY_ADMIN = 10
};

}  // namespace updater

#endif  // CHROME_UPDATER_TEST_TEST_APP_CONSTANTS_H_
