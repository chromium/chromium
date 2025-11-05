// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_
#define CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_

#include "base/time/time.h"
#include "url/gurl.h"

class Profile;

namespace waap {

// Returns true if the given URL is the initial WebUI scheme.
// This is only relevant on non-Android platforms.
// TODO(crbug.com/448794588): Some callers of this function assume that
// `WaapUIMetricsService` is available when this returns true. This
// assumption is no longer valid as the service is now gated by
// features::kInitialWebUIMetrics. Introduce a new helper function,
// e.g., ShouldLogMetricsForInitialWebUI(), that checks
// features::kInitialWebUIMetrics and update those callers.
bool IsForInitialWebUI(const GURL& url);

// Returns true if the WaapUIMetricsService and related metrics logging are
// enabled.
// This is intentionally separate from IsForInitialWebUI() because when enabled,
// the UI metrics should be logged for the UI views that are relevant to WaaP
// experiment, which includes both the existing C++ version (not a InitialWebUI)
// and the WebUI version.
bool IsInitialWebUIMetricsLoggingEnabled();

// Records the presentation time of the first paint for the browser window.
// This function ensures the metric is recorded only once per browser process.
void RecordBrowserWindowFirstPresentation(Profile* profile,
                                          base::TimeTicks presentation_time);
}  // namespace waap

#endif  // CHROME_BROWSER_UI_WAAP_WAAP_UTILS_H_
