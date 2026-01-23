// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/generated_icon_fix_result.h"

#include <ostream>

namespace web_app {

std::ostream& operator<<(std::ostream& os, GeneratedIconFixResult result) {
  switch (result) {
    case GeneratedIconFixResult::kAppUninstalled:
      return os << "kAppUninstalled";
    case GeneratedIconFixResult::kShutdown:
      return os << "kShutdown";
    case GeneratedIconFixResult::kDownloadFailure:
      return os << "kDownloadFailure";
    case GeneratedIconFixResult::kStillGenerated:
      return os << "kStillGenerated";
    case GeneratedIconFixResult::kWriteFailure:
      return os << "kWriteFailure";
    case GeneratedIconFixResult::kSuccess:
      return os << "kSuccess";
  }
}

}  // namespace web_app
