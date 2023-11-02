// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

void RecordTabStripUIOpenHistogram(TabStripUIOpenAction action) {
  UMA_HISTOGRAM_ENUMERATION("WebUITabStrip.OpenAction", action);
}

void RecordTabStripUICloseHistogram(TabStripUICloseAction action) {
  UMA_HISTOGRAM_ENUMERATION("WebUITabStrip.CloseAction", action);
}

void RecordTabStripUIOpenDurationHistogram(base::TimeDelta duration) {
  // It's unlikely a user would spend <0.5s in the tab strip, so those
  // durations probably correspond to accidentally opening and quickly
  // closing it. Hence it's a reasonable lower bound. 1 minute is a
  // fairly arbitrary upper bound.
  UMA_HISTOGRAM_CUSTOM_TIMES("WebUITabStrip.OpenDuration", duration,
                             base::Milliseconds(500), base::Minutes(1), 50);
}
