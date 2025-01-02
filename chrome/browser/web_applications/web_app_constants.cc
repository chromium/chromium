// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_constants.h"

#include <ostream>
#include <string>

#include "components/webapps/browser/installable/installable_metrics.h"

namespace web_app {

const char kRunOnOsLoginModeWindowed[] = "windowed";

std::ostream& operator<<(std::ostream& os, RunOnOsLoginMode mode) {
  switch (mode) {
    case RunOnOsLoginMode::kWindowed:
      return os << "windowed";
    case RunOnOsLoginMode::kMinimized:
      return os << "minimized";
    case RunOnOsLoginMode::kNotRun:
      return os << "not run";
  }
}

std::ostream& operator<<(std::ostream& os, ApiApprovalState state) {
  switch (state) {
    case ApiApprovalState::kAllowed:
      return os << "Allowed";
    case ApiApprovalState::kDisallowed:
      return os << "Disallowed";
    case ApiApprovalState::kRequiresPrompt:
      return os << "RequiresPrompt";
  }
}

}  // namespace web_app
