// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"

namespace companion::switches {

const char kDisableCheckUserPermissionsForCompanion[] =
    "disable-checking-companion-user-permissions";

bool ShouldOverrideCheckingUserPermissionsForCompanion() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kDisableCheckUserPermissionsForCompanion);
}

}  // namespace companion::switches
