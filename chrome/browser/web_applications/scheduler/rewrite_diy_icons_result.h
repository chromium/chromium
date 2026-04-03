// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_REWRITE_DIY_ICONS_RESULT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_REWRITE_DIY_ICONS_RESULT_H_

#include <iosfwd>

#include "base/functional/callback_forward.h"

namespace web_app {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with RewriteIconResult
// in tools/metrics/histograms/metadata/webapps/enums.xml.
// LINT.IfChange(RewriteIconResult)
enum class RewriteIconResult {
  kUnexpectedAppStateChange = 0,
  kUpdateSucceeded = 1,
  kShortcutInfoFetchFailed = 2,
  kUpdateShortcutFailed = 3,
  kMaxValue = kUpdateShortcutFailed
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:RewriteIconResult)

using RewriteIconResultCallback = base::OnceCallback<void(RewriteIconResult)>;

std::ostream& operator<<(std::ostream& os, RewriteIconResult result);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SCHEDULER_REWRITE_DIY_ICONS_RESULT_H_
