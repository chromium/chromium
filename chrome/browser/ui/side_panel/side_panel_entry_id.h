// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENTRY_ID_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENTRY_ID_H_

// Note this order matches that of the combobox options in the side panel.
// If adding a new Id here, you must also update id_to_histogram_name_map
// in side_panel_util.cc and SidePanelEntry in browser/histograms.xml.
enum class SidePanelEntryId {
  // Global Entries
  kReadingList,
  kBookmarks,
  kHistoryClusters,
  kReadAnything,
  kUserNote,
  kFeed,
  kWebView,
  kPerformance,
  // Contextual Entries
  kSideSearch,
  kLens,
  kAssistant,
  kAboutThisSite,
  kCustomizeChrome,
  kSearchCompanion,
  kShoppingInsights,
  // Extensions (nothing more should be added below here)
  kExtension
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENTRY_ID_H_
