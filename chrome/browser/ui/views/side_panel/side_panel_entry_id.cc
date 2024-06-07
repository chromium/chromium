// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"

#define SIDE_PANEL_TO_STRING_CASE_STATEMENT(entry_id, action_id, \
                                            histogram_name)      \
  case SidePanelEntryId::entry_id:                               \
    return #entry_id;
std::string SidePanelEntryIdToString(SidePanelEntryId id) {
  switch (id) { SIDE_PANEL_ENTRY_IDS(SIDE_PANEL_TO_STRING_CASE_STATEMENT) }
  NOTREACHED_IN_MIGRATION();
}
#undef SIDE_PANEL_TO_STRING_CASE_STATEMENT

#define SIDE_PANEL_HISTOGRAM_NAME_CASE_STATEMENT(entry_id, action_id, \
                                                 histogram_name)      \
  case SidePanelEntryId::entry_id:                                    \
    return histogram_name;
std::string SidePanelEntryIdToHistogramName(SidePanelEntryId id) {
  switch (id) { SIDE_PANEL_ENTRY_IDS(SIDE_PANEL_HISTOGRAM_NAME_CASE_STATEMENT) }
  NOTREACHED_IN_MIGRATION();
}
#undef SIDE_PANEL_HISTOGRAM_NAME_CASE_STATEMENT

#define SIDE_PANEL_ACTION_ID_CASE_STATEMENT(entry_id, action_id, \
                                            histogram_name)      \
  case SidePanelEntryId::entry_id:                               \
    return action_id;
std::optional<actions::ActionId> SidePanelEntryIdToActionId(
    SidePanelEntryId id) {
  switch (id) { SIDE_PANEL_ENTRY_IDS(SIDE_PANEL_ACTION_ID_CASE_STATEMENT) }
  NOTREACHED_IN_MIGRATION();
}
#undef SIDE_PANEL_ACTION_ID_CASE_STATEMENT
