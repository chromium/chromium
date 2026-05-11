// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "components/sync_sessions/features.h"

TabsFromOtherDevicesSidePanelMetrics::TabsFromOtherDevicesSidePanelMetrics() {
  base::UmaHistogramEnumeration(base::StrCat({GetHistogramPrefix(), "Events"}),
                                Event::kStartup);
}

TabsFromOtherDevicesSidePanelMetrics::~TabsFromOtherDevicesSidePanelMetrics() =
    default;

void TabsFromOtherDevicesSidePanelMetrics::Observe(SidePanelEntry* entry) {
  observation_.Observe(entry);
}

void TabsFromOtherDevicesSidePanelMetrics::OnEntryShown(SidePanelEntry* entry) {
  base::UmaHistogramEnumeration(base::StrCat({GetHistogramPrefix(), "Events"}),
                                Event::kOpened);
  opened_timestamp_ = base::TimeTicks::Now();
  tab_opened_ = false;
}

void TabsFromOtherDevicesSidePanelMetrics::OnEntryHidden(
    SidePanelEntry* entry) {
  base::UmaHistogramEnumeration(base::StrCat({GetHistogramPrefix(), "Events"}),
                                Event::kClosed);

  if (!opened_timestamp_.is_null()) {
    base::UmaHistogramLongTimes(
        base::StrCat({GetHistogramPrefix(), "TimeSpentOpen"}),
        base::TimeTicks::Now() - opened_timestamp_);
  }
}

void TabsFromOtherDevicesSidePanelMetrics::RecordTabOpened() {
  base::UmaHistogramEnumeration(base::StrCat({GetHistogramPrefix(), "Events"}),
                                Event::kTabOpened);

  if (!tab_opened_ && !opened_timestamp_.is_null()) {
    base::UmaHistogramTimes(
        base::StrCat({GetHistogramPrefix(), "TimeToFirstTab"}),
        base::TimeTicks::Now() - opened_timestamp_);
    tab_opened_ = true;
  }
}

base::WeakPtr<TabsFromOtherDevicesSidePanelMetrics>
TabsFromOtherDevicesSidePanelMetrics::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::string TabsFromOtherDevicesSidePanelMetrics::GetHistogramPrefix() const {
  const bool show_screenshots =
      base::FeatureList::IsEnabled(sync_sessions::kSyncTabScreenshots);
  return base::StrCat({"Sync.TabsFromOtherDevicesSidePanel.",
                       show_screenshots ? "Screenshot." : "List."});
}
