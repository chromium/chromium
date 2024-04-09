// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_STATS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_STATS_H_

namespace tab_groups {

class SavedTabGroupModel;

namespace stats {

// Records metrics about the state of model such as the number of saved groups,
// the number of tabs in each group, and more.
void RecordSavedTabGroupMetrics(SavedTabGroupModel* model);

}  // namespace stats
}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_STATS_H_
