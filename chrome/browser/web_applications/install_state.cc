// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/install_state.h"

#include <ostream>

namespace web_app {

std::ostream& operator<<(std::ostream& os, const InstallState& state) {
  switch (state) {
    case InstallState::kSuggestedFromAnotherDevice:
      return os << "kSuggestedFromAnotherDevice";
    case InstallState::kInstalledWithoutOsIntegration:
      return os << "kInstalledWithoutOsIntegration";
    case InstallState::kInstalledWithOsIntegration:
      return os << "kInstalledWithOsIntegration";
  }
}

}  // namespace web_app
