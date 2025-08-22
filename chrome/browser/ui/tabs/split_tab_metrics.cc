// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace split_tabs {
void RecordSplitTabCreated(SplitTabCreatedSource source) {
  base::UmaHistogramEnumeration("TabStrip.SplitView.Created", source);
}

void LogSplitViewCreatedUKM(const TabStripModel* tab_strip_model,
                            const SplitTabId split_id) {
  SplitTabData* split_data = tab_strip_model->GetSplitData(split_id);
  std::vector<tabs::TabInterface*> split_tabs = split_data->ListTabs();
  CHECK(split_tabs.size() == 2);

  int64_t split_event_id = base::RandUint64();

  for (tabs::TabInterface* split_tab : split_tabs) {
    ukm::builders::SplitView_Created(
        split_tab->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId())
        .SetSplitEventId(split_event_id)
        .SetSplitViewLayout(
            static_cast<int>(split_data->visual_data()->split_layout()))
        .Record(ukm::UkmRecorder::Get());
  }
}

void LogSplitViewUpdatedUKM(const TabStripModel* tab_strip_model,
                            const SplitTabId split_id) {
  SplitTabData* split_data = tab_strip_model->GetSplitData(split_id);
  std::vector<tabs::TabInterface*> split_tabs = split_data->ListTabs();
  CHECK(split_tabs.size() == 2);

  int64_t split_event_id = base::RandUint64();

  for (tabs::TabInterface* split_tab : split_tabs) {
    ukm::builders::SplitView_Updated(
        split_tab->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId())
        .SetSplitEventId(split_event_id)
        .SetSplitViewLayout(
            static_cast<int>(split_data->visual_data()->split_layout()))
        .Record(ukm::UkmRecorder::Get());
  }
}
}  // namespace split_tabs
