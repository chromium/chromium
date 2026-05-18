// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TABS_FROM_OTHER_DEVICES_SIDE_PANEL_METRICS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TABS_FROM_OTHER_DEVICES_SIDE_PANEL_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"

class SidePanelEntry;

// This class handles recording UMA histograms for the TabsFromOtherDevices
// side panel feature.
class TabsFromOtherDevicesSidePanelMetrics : public SidePanelEntryObserver {
 public:
  // Events to be recorded in the enum histogram.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(TabsFromOtherDevicesSidePanelEvent)
  enum class Event {
    kStartup = 0,
    kOpened = 1,
    kClosed = 2,
    kTabOpened = 3,
    kMaxValue = kTabOpened,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/sync/enums.xml:TabsFromOtherDevicesSidePanelEvent)

  TabsFromOtherDevicesSidePanelMetrics();
  TabsFromOtherDevicesSidePanelMetrics(
      const TabsFromOtherDevicesSidePanelMetrics&) = delete;
  TabsFromOtherDevicesSidePanelMetrics& operator=(
      const TabsFromOtherDevicesSidePanelMetrics&) = delete;
  ~TabsFromOtherDevicesSidePanelMetrics() override;

  void Observe(SidePanelEntry* entry);

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  bool HasRecordedTabCount() const;
  void RecordTabCountOnOpen(size_t device_count,
                            size_t total_tab_count,
                            size_t active_device_tab_count);

  void RecordTabOpened(size_t device_index);

  base::WeakPtr<TabsFromOtherDevicesSidePanelMetrics> GetWeakPtr();

 private:
  const std::string histogram_prefix_;

  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver> observation_{
      this};

  base::TimeTicks opened_timestamp_;
  bool has_recorded_tab_count_ = false;
  bool tab_opened_ = false;

  base::WeakPtrFactory<TabsFromOtherDevicesSidePanelMetrics> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_TABS_FROM_OTHER_DEVICES_TABS_FROM_OTHER_DEVICES_SIDE_PANEL_METRICS_H_
