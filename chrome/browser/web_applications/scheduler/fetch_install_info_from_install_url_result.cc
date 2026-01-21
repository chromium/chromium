// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/scheduler/fetch_install_info_from_install_url_result.h"

#include <ostream>

namespace web_app {

std::ostream& operator<<(std::ostream& os, FetchInstallInfoResult result) {
  switch (result) {
    case FetchInstallInfoResult::kAppInfoObtained:
      return os << "kAppInfoObtained";
    case FetchInstallInfoResult::kWebContentsWasDestroyed:
      return os << "kWebContentsWasDestroyed";
    case FetchInstallInfoResult::kUrlLoadingFailure:
      return os << "kUrlLoadingFailure";
    case FetchInstallInfoResult::kNoValidManifest:
      return os << "kNoValidManifest";
    case FetchInstallInfoResult::kWrongManifestId:
      return os << "kWrongManifestId";
    case FetchInstallInfoResult::kFailure:
      return os << "kFailure";
    case FetchInstallInfoResult::kShutdown:
      return os << "kShutdown";
  }
}

}  // namespace web_app
