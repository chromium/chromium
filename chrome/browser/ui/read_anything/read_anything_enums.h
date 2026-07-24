// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENUMS_H_
#define CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENUMS_H_

#include <optional>

#include "base/notreached.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"

// TODO (crbug.com/533115262): Replace ReadAnythingOpenTrigger type with
// read_anything::mojom::ReadAnythingOpenTrigger type.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ReadAnythingOpenTrigger)
enum class ReadAnythingOpenTrigger {
  kAppMenu = 0,
  kMinValue = kAppMenu,
  kReadAnythingContextMenu = 1,
  kReadAnythingNavigationThrottle = 2,
  kPinnedSidePanelEntryToolbarButton = 3,
  kOmniboxChip = 4,
  kTabSwitch = 5,
  kReadAnythingTogglePresentationButton = 6,
  kKeyboardShortcut = 7,
  kListenToThisPageContextMenu = 8,
  kUnknown = 9,
  kMaxValue = kUnknown,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:ReadAnythingOpenTrigger)

enum class ReadAnythingCloseReason {
  kClosedByUser = 0,
  kTabSwitched = 1,
  kPageChanged = 2,
  kToggledPresentation = 3,
  kRendererCrashed = 4,
  kControllerDestroyed = 5,
  kPageChangedSoftNavigation = 6,  // When Single Page Application "soft
                                   // navigation" page change is detected
  kMaxValue = kPageChangedSoftNavigation,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ReadAnythingOmniboxChipDecision)
enum class ReadAnythingOmniboxChipDecision {
  kShowArticle = 0,
  kShowPdf = 1,
  kHideAppWindow = 2,
  kHideNonHttp = 3,
  kHideDenyList = 4,
  kHideOptimizationGuide = 5,
  kHideReadability = 6,
  kHideShortPdf = 7,
  kHideLowAlphabeticPdf = 8,
  kMaxValue = kHideLowAlphabeticPdf,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:ReadAnythingOmniboxChipDecision)

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
    case ReadAnythingOpenTrigger::kTabSwitch:
      return SidePanelOpenTrigger::kTabChanged;
    case ReadAnythingOpenTrigger::kReadAnythingTogglePresentationButton:
      return SidePanelOpenTrigger::kReadAnythingTogglePresentationButton;
    case ReadAnythingOpenTrigger::kKeyboardShortcut:
      return SidePanelOpenTrigger::kReadAnythingKeyboardShortcut;
    case ReadAnythingOpenTrigger::kListenToThisPageContextMenu:
      return SidePanelOpenTrigger::kReadAnythingListenToThisPageContextMenu;
    case ReadAnythingOpenTrigger::kUnknown:
      return SidePanelOpenTrigger::kReadAnythingUnknown;
  }
}

inline ReadAnythingOpenTrigger SidePanelToReadAnythingOpenTrigger(
    SidePanelOpenTrigger trigger) {
  switch (trigger) {
    case SidePanelOpenTrigger::kAppMenu:
      return ReadAnythingOpenTrigger::kAppMenu;
    case SidePanelOpenTrigger::kReadAnythingContextMenu:
      return ReadAnythingOpenTrigger::kReadAnythingContextMenu;
    case SidePanelOpenTrigger::kReadAnythingNavigationThrottle:
      return ReadAnythingOpenTrigger::kReadAnythingNavigationThrottle;
    case SidePanelOpenTrigger::kToolbarButton:
    case SidePanelOpenTrigger::kPinnedEntryToolbarButton:
    case SidePanelOpenTrigger::kOverflowMenu:
      return ReadAnythingOpenTrigger::kPinnedSidePanelEntryToolbarButton;
    case SidePanelOpenTrigger::kReadAnythingOmniboxChip:
      return ReadAnythingOpenTrigger::kOmniboxChip;
    case SidePanelOpenTrigger::kTabChanged:
      return ReadAnythingOpenTrigger::kTabSwitch;
    case SidePanelOpenTrigger::kReadAnythingTogglePresentationButton:
      return ReadAnythingOpenTrigger::kReadAnythingTogglePresentationButton;
    case SidePanelOpenTrigger::kReadAnythingKeyboardShortcut:
      return ReadAnythingOpenTrigger::kKeyboardShortcut;
    case SidePanelOpenTrigger::kReadAnythingListenToThisPageContextMenu:
      return ReadAnythingOpenTrigger::kListenToThisPageContextMenu;
    case SidePanelOpenTrigger::kReadAnythingUnknown:
    case SidePanelOpenTrigger::kSideSearchPageAction:
    case SidePanelOpenTrigger::kNotesInPageContextMenu:
    case SidePanelOpenTrigger::kComboboxSelected:
    case SidePanelOpenTrigger::kSidePanelEntryDeregistered:
    case SidePanelOpenTrigger::kIPHSideSearchAutoTrigger:
    case SidePanelOpenTrigger::kContextMenuSearchOption:
    case SidePanelOpenTrigger::kExtensionEntryRegistered:
    case SidePanelOpenTrigger::kBookmarkBar:
    case SidePanelOpenTrigger::kOpenedInNewTabFromSidePanel:
    case SidePanelOpenTrigger::kExtension:
    case SidePanelOpenTrigger::kNewTabPage:
    case SidePanelOpenTrigger::kReadingListToast:
    case SidePanelOpenTrigger::kNewTabFooter:
    case SidePanelOpenTrigger::kNewTabPageCustomizationPromo:
    case SidePanelOpenTrigger::kNewTabPageAutomaticCustomizeChrome:
#if BUILDFLAG(IS_ANDROID)
    case SidePanelOpenTrigger::kWindowResized:
#endif
    case SidePanelOpenTrigger::kGlicOpened:
    case SidePanelOpenTrigger::kContextualTasks:
    case SidePanelOpenTrigger::kUnknown:
      return ReadAnythingOpenTrigger::kUnknown;
  }
  NOTREACHED();
}

}  // namespace read_anything

#endif  // CHROME_BROWSER_UI_READ_ANYTHING_READ_ANYTHING_ENUMS_H_
