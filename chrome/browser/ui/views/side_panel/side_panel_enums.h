// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENUMS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENUMS_H_

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. SidePanelOpenTrigger in
// tools/metrics/histograms/enums.xml should also be updated when changed
// here.
enum class SidePanelOpenTrigger {
  kToolbarButton = 0,
  kMinValue = kToolbarButton,
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
  // kReadAnythingOmniboxIcon = 15, (deprecated)
  kReadAnythingNavigationThrottle = 16,
  kOverflowMenu = 17,
  kExtension = 18,
  kNewTabPage = 19,
  kReadingListToast = 20,
  kMaxValue = kReadingListToast,
};

enum class SidePanelContentState {
  // Content is ready to show and will influence side panel visibility.
  kReadyToShow = 0,
  // Side panel content should be hidden, either immediately if other content
  // is causing the side panel to remain open or after the side panel has been
  // closed.
  kReadyToHide = 1,
  // Content is ready to show, will influence side panel visibility, and show
  // immediately without any animations.
  kShowImmediately = 2,
  // Side panel content should be hidden immediately with no animations.
  kHideImmediately = 3,
};

enum class SidePanelEntryHideReason {
  // Side panel entry was hidden because the side panel was closed.
  kSidePanelClosed = 0,
  // Side panel entry was hidden because another entry was loaded into the
  // side panel.
  kReplaced = 1,
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENUMS_H_
