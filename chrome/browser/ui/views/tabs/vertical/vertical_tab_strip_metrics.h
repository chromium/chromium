// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_METRICS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_METRICS_H_

namespace tabs {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(VerticalTabStripEntryPoint)
enum class VerticalTabStripEntryPoint {
  kAppMenu = 0,
  kSettings = 1,
  kSystemContextMenu = 2,
  kTabContextMenu = 3,
  kMacViewMenu = 4,
  kMaxValue = kMacViewMenu,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:VerticalTabStripEntryPoint)

void RecordVerticalTabStripModeChanged(bool is_vertical,
                                       VerticalTabStripEntryPoint entry_point);

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_METRICS_H_
