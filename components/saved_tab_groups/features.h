// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_FEATURES_H_
#define COMPONENTS_SAVED_TAB_GROUPS_FEATURES_H_

#include "base/feature_list.h"

namespace tab_groups {
BASE_DECLARE_FEATURE(kTabGroupSyncAndroid);

BASE_DECLARE_FEATURE(kTabGroupSyncForceOff);

BASE_DECLARE_FEATURE(kAndroidTabGroupStableIds);

BASE_DECLARE_FEATURE(kTabGroupsSaveV2);

BASE_DECLARE_FEATURE(kTabGroupsSaveUIUpdate);

BASE_DECLARE_FEATURE(kTabGroupSyncUno);

extern bool IsTabGroupsSaveV2Enabled();

extern bool IsTabGroupsSaveUIUpdateEnabled();

extern bool ShouldCloseAllTabGroupsOnSignOut();

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_FEATURES_H_
