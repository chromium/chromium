// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_metrics.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace split_tabs {
namespace {
std::string_view GetMetricsSuffixForSource(SplitTabCreatedSource source) {
  // These strings are persisted to logs. Entries should not be changed.
  switch (source) {
    case SplitTabCreatedSource::kToolbarButton:
      return "ToolbarButton";
    case SplitTabCreatedSource::kDragAndDropLink:
      return "DragAndDropLink";
    case SplitTabCreatedSource::kDragAndDropTab:
      return "DragAndDropTab";
    case SplitTabCreatedSource::kBookmarkContextMenu:
      return "BookmarkContextMenu";
    case SplitTabCreatedSource::kLinkContextMenu:
      return "LinkContextMenu";
    case SplitTabCreatedSource::kTabContextMenu:
      return "TabContextMenu";
    case SplitTabCreatedSource::kDuplicateSplit:
      return "DuplicateSplit";
    case SplitTabCreatedSource::kExtensionsApi:
      return "ExtensionsApi";
    case SplitTabCreatedSource::kWhatsNew:
      return "WhatsNew";
    case SplitTabCreatedSource::kKeyboardShortcut:
      return "KeyboardShortcut";
    case SplitTabCreatedSource::kNewTabButton:
      return "NewTabButton";
  }
}
}  // namespace

void RecordSplitTabCreated(SplitTabCreatedSource source) {
  base::UmaHistogramEnumeration("TabStrip.SplitView.Created", source);
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({"SplitViewCreated_", GetMetricsSuffixForSource(source)})
          .c_str()));
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
