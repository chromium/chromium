// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_GENERATED_ICON_FIX_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_GENERATED_ICON_FIX_RESULT_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"

namespace web_app {

// Used by metrics.
// LINT.IfChange(GeneratedIconFixResult)
enum class GeneratedIconFixResult {
  kAppUninstalled = 0,
  kShutdown = 1,
  kDownloadFailure = 2,
  kStillGenerated = 3,
  kWriteFailure = 4,
  kSuccess = 5,

  kMaxValue = kSuccess,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:GeneratedIconFixResult)

using GeneratedIconFixCallback =
    base::OnceCallback<void(GeneratedIconFixResult)>;

std::ostream& operator<<(std::ostream& os, GeneratedIconFixResult result);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_GENERATED_ICON_FIX_RESULT_H_
