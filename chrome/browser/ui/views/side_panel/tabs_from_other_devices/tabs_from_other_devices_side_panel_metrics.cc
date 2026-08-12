// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"

namespace {

constexpr char kHistogramPrefix[] = "Sync.TabsFromOtherDevicesSidePanel.List.";

}  // namespace

TabsFromOtherDevicesSidePanelMetrics::TabsFromOtherDevicesSidePanelMetrics() {
  base::UmaHistogramEnumeration(base::StrCat({kHistogramPrefix, "Events"}),
                                Event::kStartup);
}

TabsFromOtherDevicesSidePanelMetrics::~TabsFromOtherDevicesSidePanelMetrics() =
    default;

void TabsFromOtherDevicesSidePanelMetrics::Observe(SidePanelEntry* entry) {
  observation_.Observe(entry);
}

void TabsFromOtherDevicesSidePanelMetrics::OnEntryShown(SidePanelEntry* entry) {
  base::UmaHistogramEnumeration(base::StrCat({kHistogramPrefix, "Events"}),
                                Event::kOpened);
  opened_timestamp_ = base::TimeTicks::Now();
  tab_opened_ = false;
  has_recorded_tab_count_ = false;
}

void TabsFromOtherDevicesSidePanelMetrics::OnEntryHidden(
    SidePanelEntry* entry) {
  base::UmaHistogramEnumeration(base::StrCat({kHistogramPrefix, "Events"}),
                                Event::kClosed);

  if (!opened_timestamp_.is_null()) {
    base::UmaHistogramLongTimes(
        base::StrCat({kHistogramPrefix, "TimeSpentOpen"}),
        base::TimeTicks::Now() - opened_timestamp_);
  }
}

bool TabsFromOtherDevicesSidePanelMetrics::HasRecordedTabCount() const {
  return has_recorded_tab_count_;
}

void TabsFromOtherDevicesSidePanelMetrics::RecordTabCountOnOpen(
    size_t device_count,
    size_t total_tab_count,
    size_t active_device_tab_count) {
  base::UmaHistogramCounts100(
      base::StrCat({kHistogramPrefix, "DeviceCountOnOpen"}), device_count);
  base::UmaHistogramCounts1000(
      base::StrCat({kHistogramPrefix, "TabCountOnOpen.Total"}),
      total_tab_count);
  base::UmaHistogramCounts1000(
      base::StrCat({kHistogramPrefix, "TabCountOnOpen.ActiveDevice"}),
      active_device_tab_count);
  has_recorded_tab_count_ = true;
}

void TabsFromOtherDevicesSidePanelMetrics::RecordTabOpened(
    size_t device_index,
    size_t tab_recency_index) {
  base::UmaHistogramEnumeration(base::StrCat({kHistogramPrefix, "Events"}),
                                Event::kTabOpened);

  base::UmaHistogramExactLinear(
      base::StrCat({kHistogramPrefix, "OpenedTabDeviceIndex"}), device_index,
      10);

  base::UmaHistogramCounts1000(
      base::StrCat({kHistogramPrefix, "OpenedTabRecencyIndex"}),
      tab_recency_index);

  if (!tab_opened_ && !opened_timestamp_.is_null()) {
    base::UmaHistogramTimes(base::StrCat({kHistogramPrefix, "TimeToFirstTab"}),
                            base::TimeTicks::Now() - opened_timestamp_);
    tab_opened_ = true;
  }
}

base::WeakPtr<TabsFromOtherDevicesSidePanelMetrics>
TabsFromOtherDevicesSidePanelMetrics::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
