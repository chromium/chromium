// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/isolated_web_app_apply_update_result.h"

namespace web_app {

std::ostream& operator<<(std::ostream& os,
                         const IsolatedWebAppApplyUpdateCommandError& error) {
  return os << "IsolatedWebAppApplyUpdateCommandError { "
               "message = \""
            << error.message << "\" }.";
}

}  // namespace web_app
