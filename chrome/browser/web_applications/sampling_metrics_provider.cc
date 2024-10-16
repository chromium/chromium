// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/sampling_metrics_provider.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

namespace web_app {

namespace {

// Decreasing this number will improve accuracy at the expense of more frequent
// client-side work.
constexpr int kTimerIntervalInSeconds = 5 * 60;

}  // namespace

SamplingMetricsProvider::SamplingMetricsProvider() {
  timer_.Start(FROM_HERE, base::Seconds(kTimerIntervalInSeconds),
               base::BindRepeating(&SamplingMetricsProvider::EmitMetrics));
}

SamplingMetricsProvider::~SamplingMetricsProvider() = default;

void SamplingMetricsProvider::EmitMetrics() {
  // Total number of PWAs, including backgrounded.
  int pwas_in_use = 0;

  // Whether the foreground window has a PWA as its foreground tab.
  bool pwas_in_active_use = false;

  for (BrowserWindowInterface* browser : GetAllBrowserWindowInterfaces()) {
    // If this is a standalone app window.
    if (browser->GetAppBrowserController()) {
      ++pwas_in_use;
      if (browser->IsActive()) {
        pwas_in_active_use = true;
      }
    }

    // If this is a PWA-tab in a normal browser window.
    if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {
      // TODO(https://crbug.com/358404364): Implement.
    }
  }

  UMA_HISTOGRAM_COUNTS_100("WebApp.Engagement2.Count", pwas_in_use);
  UMA_HISTOGRAM_BOOLEAN("WebApp.Engagement2.Active", pwas_in_active_use);
}

}  // namespace web_app
