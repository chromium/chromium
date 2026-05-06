// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENUMS_UTILS_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENUMS_UTILS_H_

#include <string>

#include "base/notreached.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"

inline std::string ToString(SidePanelOpenTrigger trigger) {
  switch (trigger) {
    case SidePanelOpenTrigger::kToolbarButton:
      return "ToolbarButton";
    case SidePanelOpenTrigger::kSideSearchPageAction:
      return "SideSearchPageAction";
    case SidePanelOpenTrigger::kNotesInPageContextMenu:
      return "NotesInPageContextMenu";
    case SidePanelOpenTrigger::kComboboxSelected:
      return "ComboboxSelected";
    case SidePanelOpenTrigger::kTabChanged:
      return "TabChanged";
    case SidePanelOpenTrigger::kSidePanelEntryDeregistered:
      return "SidePanelEntryDeregistered";
    case SidePanelOpenTrigger::kIPHSideSearchAutoTrigger:
      return "IPHSideSearchAutoTrigger";
    case SidePanelOpenTrigger::kContextMenuSearchOption:
      return "ContextMenuSearchOption";
    case SidePanelOpenTrigger::kReadAnythingContextMenu:
      return "ReadAnythingContextMenu";
    case SidePanelOpenTrigger::kExtensionEntryRegistered:
      return "ExtensionEntryRegistered";
    case SidePanelOpenTrigger::kBookmarkBar:
      return "BookmarkBar";
    case SidePanelOpenTrigger::kPinnedEntryToolbarButton:
      return "PinnedEntryToolbarButton";
    case SidePanelOpenTrigger::kAppMenu:
      return "AppMenu";
    case SidePanelOpenTrigger::kOpenedInNewTabFromSidePanel:
      return "OpenedInNewTabFromSidePanel";
    case SidePanelOpenTrigger::kReadAnythingNavigationThrottle:
      return "ReadAnythingNavigationThrottle";
    case SidePanelOpenTrigger::kOverflowMenu:
      return "OverflowMenu";
    case SidePanelOpenTrigger::kExtension:
      return "Extension";
    case SidePanelOpenTrigger::kNewTabPage:
      return "NewTabPage";
    case SidePanelOpenTrigger::kReadingListToast:
      return "ReadingListToast";
    case SidePanelOpenTrigger::kNewTabFooter:
      return "NewTabFooter";
    case SidePanelOpenTrigger::kNewTabPageCustomizationPromo:
      return "NewTabPageCustomizationPromo";
    case SidePanelOpenTrigger::kNewTabPageAutomaticCustomizeChrome:
      return "NewTabPageAutomaticCustomizeChrome";
    case SidePanelOpenTrigger::kReadAnythingOmniboxChip:
      return "ReadAnythingOmniboxChip";
    case SidePanelOpenTrigger::kReadAnythingTogglePresentationButton:
      return "ReadAnythingTogglePresentationButton";
    case SidePanelOpenTrigger::kReadAnythingKeyboardShortcut:
      return "ReadAnythingKeyboardShortcut";
  }
  NOTREACHED();
}

inline std::string ToString(SidePanelContentState state) {
  switch (state) {
    case SidePanelContentState::kReadyToShow:
      return "ReadyToShow";
    case SidePanelContentState::kReadyToHide:
      return "ReadyToHide";
    case SidePanelContentState::kShowImmediately:
      return "ShowImmediately";
    case SidePanelContentState::kHideImmediately:
      return "HideImmediately";
  }
  NOTREACHED();
}

inline std::string ToString(SidePanelEntryHideReason reason) {
  switch (reason) {
    case SidePanelEntryHideReason::kSidePanelClosed:
      return "SidePanelClosed";
    case SidePanelEntryHideReason::kReplaced:
      return "Replaced";
    case SidePanelEntryHideReason::kBackgrounded:
      return "Backgrounded";
#if BUILDFLAG(IS_ANDROID)
    case SidePanelEntryHideReason::kWindowResized:
      return "WindowResized";
#endif
  }
  NOTREACHED();
}

inline std::string ToString(SidePanelType type) {
  switch (type) {
    case SidePanelType::kContent:
      return "Content";
    case SidePanelType::kToolbar:
      return "Toolbar";
  }
  NOTREACHED();
}

inline std::string ToString(SidePanelState state) {
  switch (state) {
    case SidePanelState::kClosed:
      return "Closed";
    case SidePanelState::kOpening:
      return "Opening";
    case SidePanelState::kShown:
      return "Shown";
    case SidePanelState::kClosing:
      return "Closing";
  }
  NOTREACHED();
}

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENUMS_UTILS_H_
