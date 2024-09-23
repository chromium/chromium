// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/proto/proto_helpers.h"

#include <ostream>

#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"

namespace web_app::proto {

std::ostream& operator<<(std::ostream& os, const proto::InstallState& state) {
  switch (state) {
    case InstallState::SUGGESTED_FROM_ANOTHER_DEVICE:
      return os << "SUGGESTED_FROM_ANOTHER_DEVICE";
    case InstallState::INSTALLED_WITHOUT_OS_INTEGRATION:
      return os << "INSTALLED_WITHOUT_OS_INTEGRATION";
    case InstallState::INSTALLED_WITH_OS_INTEGRATION:
      return os << "INSTALLED_WITH_OS_INTEGRATION";
  }
}

}  // namespace web_app::proto
