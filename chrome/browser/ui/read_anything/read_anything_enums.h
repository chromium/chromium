// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENUMS_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENUMS_H_

#include <optional>

#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. ReadAnythingOpenTrigger in
// tools/metrics/histograms/enums.xml should also be updated when changed
// here.
enum class ReadAnythingOpenTrigger {
  kAppMenu = 0,
  kMinValue = kAppMenu,
  kReadAnythingContextMenu = 1,
  kReadAnythingNavigationThrottle = 2,
  kPinnedSidePanelEntryToolbarButton = 3,
  kOmniboxChip = 4,
  kMaxValue = kOmniboxChip,
};

namespace read_anything {

inline SidePanelOpenTrigger ReadAnythingToSidePanelOpenTrigger(
    ReadAnythingOpenTrigger trigger) {
  switch (trigger) {
    case ReadAnythingOpenTrigger::kAppMenu:
      return SidePanelOpenTrigger::kAppMenu;
    case ReadAnythingOpenTrigger::kReadAnythingContextMenu:
      return SidePanelOpenTrigger::kReadAnythingContextMenu;
    case ReadAnythingOpenTrigger::kReadAnythingNavigationThrottle:
      return SidePanelOpenTrigger::kReadAnythingNavigationThrottle;
    case ReadAnythingOpenTrigger::kPinnedSidePanelEntryToolbarButton:
      return SidePanelOpenTrigger::kPinnedEntryToolbarButton;
    case ReadAnythingOpenTrigger::kOmniboxChip:
      return SidePanelOpenTrigger::kReadAnythingOmniboxChip;
  }
}

inline std::optional<ReadAnythingOpenTrigger>
SidePanelToReadAnythingOpenTrigger(SidePanelOpenTrigger trigger) {
  switch (trigger) {
    case SidePanelOpenTrigger::kAppMenu:
      return ReadAnythingOpenTrigger::kAppMenu;
    case SidePanelOpenTrigger::kReadAnythingContextMenu:
      return ReadAnythingOpenTrigger::kReadAnythingContextMenu;
    case SidePanelOpenTrigger::kReadAnythingNavigationThrottle:
      return ReadAnythingOpenTrigger::kReadAnythingNavigationThrottle;
    case SidePanelOpenTrigger::kPinnedEntryToolbarButton:
      return ReadAnythingOpenTrigger::kPinnedSidePanelEntryToolbarButton;
    case SidePanelOpenTrigger::kReadAnythingOmniboxChip:
      return ReadAnythingOpenTrigger::kOmniboxChip;
    default:
      return std::optional<ReadAnythingOpenTrigger>();
  }
}

}  // namespace read_anything

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENUMS_H_
