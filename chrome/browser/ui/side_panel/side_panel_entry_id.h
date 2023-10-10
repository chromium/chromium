// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENTRY_ID_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENTRY_ID_H_

#include <string>

#include "base/notreached.h"

// Note this order matches that of the combobox options in the side panel.
// If adding a new Id here, you must also update id_to_histogram_name_map
// in side_panel_util.cc and SidePanelEntry in browser/histograms.xml.

#define SIDE_PANEL_ENTRY_IDS(V)                              \
  /* Global Entries */                                       \
  V(kReadingList)                                            \
  V(kBookmarks)                                              \
  V(kHistoryClusters)                                        \
  V(kReadAnything)                                           \
  V(kUserNote)                                               \
  V(kFeed)                                                   \
  V(kWebView)                                                \
  V(kPerformance)                                            \
  /* Contextual Entries */                                   \
  V(kSideSearch)                                             \
  V(kLens)                                                   \
  V(kAssistant)                                              \
  V(kAboutThisSite)                                          \
  V(kCustomizeChrome)                                        \
  V(kSearchCompanion)                                        \
  V(kShoppingInsights)                                       \
  /* Extensions (nothing more should be added below here) */ \
  V(kExtension)

#define SIDE_PANEL_ENTRY_ID_ENUM(id) id,
enum class SidePanelEntryId { SIDE_PANEL_ENTRY_IDS(SIDE_PANEL_ENTRY_ID_ENUM) };
#undef SIDE_PANEL_ENTRY_ID_ENUM

#define SIDE_PANEL_CASE_STATEMENT(id) \
  case SidePanelEntryId::id:          \
    return #id;
inline std::string SidePanelEntryIdToString(SidePanelEntryId id) {
  switch (id) { SIDE_PANEL_ENTRY_IDS(SIDE_PANEL_CASE_STATEMENT) }
  NOTREACHED();
}
#undef SIDE_PANEL_CASE_STATEMENT

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENTRY_ID_H_
