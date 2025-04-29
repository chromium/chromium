// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_NOOP_PAGE_ACTION_METRICS_RECORDER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_NOOP_PAGE_ACTION_METRICS_RECORDER_H_

#include <memory>

#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace page_actions {

// A metrics reporter implementation that does nothing.
class NoopPageActionMetricsRecorder
    : public PageActionPerActionMetricsRecorderInterface,
      public PageActionPageMetricsRecorderInterface {
 public:
  NoopPageActionMetricsRecorder() = default;
  ~NoopPageActionMetricsRecorder() override = default;

  // PageActionPerActionMetricsRecorderInterface:
  void RecordClick(PageActionTrigger trigger_source) final;

  // PageActionPageMetricsRecorderInterface:
  void Observe(PageActionModelInterface& model) final;
};

class NoopPageActionMetricsRecorderFactory
    : public PageActionMetricsRecorderFactory {
 public:
  std::unique_ptr<PageActionPerActionMetricsRecorderInterface>
  CreatePerActionMetricsRecorder(
      tabs::TabInterface& tab_interface,
      const PageActionProperties& properties,
      PageActionModelInterface& model,
      VisibleEphemeralPageActionsCountCallback
          visible_ephemeral_page_actions_count_callback) override;

  std::unique_ptr<PageActionPageMetricsRecorderInterface>
  CreatePageMetricRecorder(
      tabs::TabInterface& tab_interface,
      VisibleEphemeralPageActionsCountCallback
          visible_ephemeral_page_actions_count_callback) override;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_NOOP_PAGE_ACTION_METRICS_RECORDER_H_
