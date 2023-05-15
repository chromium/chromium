// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENUMS_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENUMS_H_

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. SidePanelOpenTrigger in
// tools/metrics/histograms/enums.xml should also be updated when changed
// here.
enum class SidePanelOpenTrigger {
  kToolbarButton = 0,
  kLensContextMenu = 1,
  kSideSearchPageAction = 2,
  kNotesInPageContextMenu = 3,
  kComboboxSelected = 4,
  kTabChanged = 5,
  kSidePanelEntryDeregistered = 6,
  kIPHSideSearchAutoTrigger = 7,
  kContextMenuSearchOption = 8,
  kReadAnythingContextMenu = 9,
  kExtensionEntryRegistered = 10,
  kBookmarkBar = 11,
  kPinnedEntryToolbarButton = 12,
  kAppMenu = 13,
  kOpenedInNewTabFromSidePanel = 14,
  kMaxValue = kOpenedInNewTabFromSidePanel,
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENUMS_H_
