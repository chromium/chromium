// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATER_SCOPE_H_
#define CHROME_UPDATER_UPDATER_SCOPE_H_

#include <ostream>

#include "base/command_line.h"
#include "chrome/updater/constants.h"

namespace updater {

// Scope of the service invocation.
enum class UpdaterScope {
  // The updater is running in the logged in user's scope.
  kUser = 1,

  // The updater is running in the system's scope.
  kSystem = 2,
};

inline UpdaterScope GetProcessScope() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kSystemSwitch)
             ? UpdaterScope::kSystem
             : UpdaterScope::kUser;
}

inline std::ostream& operator<<(std::ostream& os, UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kUser:
      return os << "User";
    case UpdaterScope::kSystem:
      return os << "System";
  }
}

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATER_SCOPE_H_
