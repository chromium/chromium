// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_LAUNCH_OR_REPARENT_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_LAUNCH_OR_REPARENT_RESULT_H_

#include <iosfwd>

namespace web_app {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(LaunchOrReparentResult)
enum class LaunchOrReparentResult {
  kReparented = 0,
  kLaunched = 1,
  kWebContentsGone = 2,
  kAppNotInstalledAsDedicatedWindow = 3,
  kShutdown = 4,
  kMaxValue = kShutdown
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:WebAppLaunchOrReparentResult)

std::ostream& operator<<(std::ostream& os, LaunchOrReparentResult result);

}  //  namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_LAUNCH_OR_REPARENT_RESULT_H_
