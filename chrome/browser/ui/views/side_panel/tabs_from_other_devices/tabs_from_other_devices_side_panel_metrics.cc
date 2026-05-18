// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "components/sync_sessions/features.h"

namespace {

std::string BuildHistogramPrefix() {
  const bool show_screenshots =
      base::FeatureList::IsEnabled(sync_sessions::kSyncTabScreenshots);
  return base::StrCat({"Sync.TabsFromOtherDevicesSidePanel.",
                       show_screenshots ? "Screenshot." : "List."});
}

}  // namespace

TabsFromOtherDevicesSidePanelMetrics::TabsFromOtherDevicesSidePanelMetrics()
    : histogram_prefix_(BuildHistogramPrefix()) {
  base::UmaHistogramEnumeration(base::StrCat({histogram_prefix_, "Events"}),
                                Event::kStartup);
}

TabsFromOtherDevicesSidePanelMetrics::~TabsFromOtherDevicesSidePanelMetrics() =
    default;

void TabsFromOtherDevicesSidePanelMetrics::Observe(SidePanelEntry* entry) {
  observation_.Observe(entry);
}

void TabsFromOtherDevicesSidePanelMetrics::OnEntryShown(SidePanelEntry* entry) {
  base::UmaHistogramEnumeration(base::StrCat({histogram_prefix_, "Events"}),
                                Event::kOpened);
  opened_timestamp_ = base::TimeTicks::Now();
  tab_opened_ = false;
  has_recorded_tab_count_ = false;
}

void TabsFromOtherDevicesSidePanelMetrics::OnEntryHidden(
    SidePanelEntry* entry) {
  base::UmaHistogramEnumeration(base::StrCat({histogram_prefix_, "Events"}),
                                Event::kClosed);

  if (!opened_timestamp_.is_null()) {
    base::UmaHistogramLongTimes(
        base::StrCat({histogram_prefix_, "TimeSpentOpen"}),
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
      base::StrCat({histogram_prefix_, "DeviceCountOnOpen"}), device_count);
  base::UmaHistogramCounts1000(
      base::StrCat({histogram_prefix_, "TabCountOnOpen.Total"}),
      total_tab_count);
  base::UmaHistogramCounts1000(
      base::StrCat({histogram_prefix_, "TabCountOnOpen.ActiveDevice"}),
      active_device_tab_count);
  has_recorded_tab_count_ = true;
}

void TabsFromOtherDevicesSidePanelMetrics::RecordTabOpened(
    size_t device_index) {
  base::UmaHistogramEnumeration(base::StrCat({histogram_prefix_, "Events"}),
                                Event::kTabOpened);

  base::UmaHistogramExactLinear(
      base::StrCat({histogram_prefix_, "OpenedTabDeviceIndex"}), device_index,
      10);

  if (!tab_opened_ && !opened_timestamp_.is_null()) {
    base::UmaHistogramTimes(base::StrCat({histogram_prefix_, "TimeToFirstTab"}),
                            base::TimeTicks::Now() - opened_timestamp_);
    tab_opened_ = true;
  }
}

base::WeakPtr<TabsFromOtherDevicesSidePanelMetrics>
TabsFromOtherDevicesSidePanelMetrics::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
