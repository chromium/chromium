// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/navigate_and_trigger_install_dialog_result.h"

#include <ostream>

namespace web_app {

std::ostream& operator<<(std::ostream& os,
                         NavigateAndTriggerInstallDialogResult result) {
  switch (result) {
    case NavigateAndTriggerInstallDialogResult::kFailure:
      return os << "kFailure";
    case NavigateAndTriggerInstallDialogResult::kAlreadyInstalled:
      return os << "kAlreadyInstalled";
    case NavigateAndTriggerInstallDialogResult::kDialogShown:
      return os << "kDialogShown";
    case NavigateAndTriggerInstallDialogResult::kShutdown:
      return os << "kShutdown";
  }
}

}  // namespace web_app
