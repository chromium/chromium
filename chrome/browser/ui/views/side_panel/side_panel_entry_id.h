// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_ID_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_ID_H_

#include <optional>
#include <string>

#include "base/notreached.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "ui/actions/action_id.h"

// Note: this order matches that of the combobox options in the side panel.
// Once provided the histogram name should not be changed since it
// is persisted to logs. When adding a new Id please add actions to
// tools/metrics/actions/actions.xml for "SidePanel.[new id name].Shown"
// since we cannot autogenerate this in actions.xml.
#define SIDE_PANEL_ENTRY_IDS(V)                                               \
  /* Global Entries */                                                        \
  V(kReadingList, kActionSidePanelShowReadingList, "ReadingList")             \
  V(kBookmarks, kActionSidePanelShowBookmarks, "Bookmarks")                   \
  V(kHistoryClusters, kActionSidePanelShowHistoryCluster, "HistoryClusters")  \
  V(kReadAnything, kActionSidePanelShowReadAnything, "ReadAnything")          \
  V(kUserNote, kActionSidePanelShowUserNote, "UserNotes")                     \
  V(kFeed, kActionSidePanelShowFeed, "Feed")                                  \
  V(kWebView, std::nullopt, "WebView")                                        \
  /* Contextual Entries */                                                    \
  V(kSideSearch, kActionSidePanelShowSideSearch, "SideSearch")                \
  V(kLens, kActionSidePanelShowLens, "Lens")                                  \
  V(kAssistant, kActionSidePanelShowAssistant, "Assistant")                   \
  V(kAboutThisSite, kActionSidePanelShowAboutThisSite, "AboutThisSite")       \
  V(kCustomizeChrome, kActionSidePanelShowCustomizeChrome, "CustomizeChrome") \
  V(kSearchCompanion, kActionSidePanelShowSearchCompanion, "Companion")       \
  V(kShoppingInsights, kActionSidePanelShowShoppingInsights,                  \
    "ShoppingInsights")                                                       \
  V(kLensOverlayResults, kActionSidePanelShowLensOverlayResults,              \
    "LensOverlayResults")                                                     \
  /* Extensions (nothing more should be added below here) */                  \
  V(kExtension, std::nullopt, "Extension")

#define SIDE_PANEL_ENTRY_ID_ENUM(entry_id, action_id, histogram_name) entry_id,
enum class SidePanelEntryId { SIDE_PANEL_ENTRY_IDS(SIDE_PANEL_ENTRY_ID_ENUM) };
#undef SIDE_PANEL_ENTRY_ID_ENUM

std::string SidePanelEntryIdToString(SidePanelEntryId id);

std::string SidePanelEntryIdToHistogramName(SidePanelEntryId id);

std::optional<actions::ActionId> SidePanelEntryIdToActionId(
    SidePanelEntryId id);

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_ENTRY_ID_H_
