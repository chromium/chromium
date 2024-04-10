// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_STATS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_STATS_H_

#include <stddef.h>

namespace tab_groups {

class SavedTabGroupModel;

namespace stats {

// Records metrics about the state of model such as the number of saved groups,
// the number of tabs in each group, and more.
void RecordSavedTabGroupMetrics(SavedTabGroupModel* model);

// Records the difference in the number of tabs between local group and the
// synced version when the local tab group is connected with the synced one.
void RecordTabCountMismatchOnConnect(size_t tabs_in_saved_group,
                                     size_t tabs_in_group);

}  // namespace stats
}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_STATS_H_
