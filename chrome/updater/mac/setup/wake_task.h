// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_SETUP_WAKE_TASK_H_
#define CHROME_UPDATER_MAC_SETUP_WAKE_TASK_H_

#include <Foundation/Foundation.h>

#include "chrome/updater/updater_scope.h"

namespace updater {

// Returns the dictionary to use for the wake launchd plist. Can return nil on
// error (e.g. if the target cannot be found).
NSDictionary* CreateWakeLaunchdPlist(UpdaterScope scope);

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_SETUP_WAKE_TASK_H_
