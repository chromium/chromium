// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/fetch_installability_for_chrome_management_result.h"

#include <ostream>

namespace web_app {

std::ostream& operator<<(std::ostream& os, InstallableCheckResult result) {
  switch (result) {
    case InstallableCheckResult::kNotInstallable:
      return os << "kNotInstallable";
    case InstallableCheckResult::kInstallable:
      return os << "kInstallable";
    case InstallableCheckResult::kAlreadyInstalled:
      return os << "kAlreadyInstalled";
  }
}

}  // namespace web_app
