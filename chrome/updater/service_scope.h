// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SERVICE_SCOPE_H_
#define CHROME_UPDATER_SERVICE_SCOPE_H_

#include "base/command_line.h"
#include "chrome/updater/constants.h"

namespace updater {

// Scope of the service invocation.
enum class ServiceScope {
  // The updater is running in the logged in user's scope.
  kUser = 1,

  // The updater is running in the system's scope.
  kSystem = 2,
};

inline ServiceScope GetProcessScope() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kSystemSwitch)
             ? ServiceScope::kSystem
             : ServiceScope::kUser;
}

}  // namespace updater

#endif  // CHROME_UPDATER_SERVICE_SCOPE_H_
