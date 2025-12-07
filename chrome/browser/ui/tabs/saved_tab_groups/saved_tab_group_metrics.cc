// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace tab_groups::saved_tab_groups::metrics {

void RecordSharedTabGroupRecallType(SharedTabGroupRecallTypeDesktop action) {
  base::UmaHistogramEnumeration("TabGroups.Shared.Recall.Desktop", action);
}

void RecordSharedTabGroupManageType(SharedTabGroupManageTypeDesktop action) {
  base::UmaHistogramEnumeration("TabGroups.Shared.Manage.Desktop", action);
}

}  // namespace tab_groups::saved_tab_groups::metrics
