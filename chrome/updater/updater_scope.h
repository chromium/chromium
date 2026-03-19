// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATER_SCOPE_H_
#define CHROME_UPDATER_UPDATER_SCOPE_H_

#include <ostream>
#include <string>

namespace updater {

// Scope of the service invocation.
enum class UpdaterScope {
  // The updater is running in the logged in user's scope.
  kUser = 1,

  // The updater is running in the system's scope.
  kSystem = 2,
};

constexpr bool IsSystemInstall(UpdaterScope scope) {
  return scope == UpdaterScope::kSystem;
}

inline std::string UpdaterScopeToString(UpdaterScope scope) {
  switch (scope) {
    case UpdaterScope::kUser:
      return "User";
    case UpdaterScope::kSystem:
      return "System";
  }
}

inline std::ostream& operator<<(std::ostream& os, UpdaterScope scope) {
  return os << UpdaterScopeToString(scope).c_str();
}

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATER_SCOPE_H_
