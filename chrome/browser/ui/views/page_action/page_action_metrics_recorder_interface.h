// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_INTERFACE_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"

namespace tabs {
class TabInterface;
}

namespace page_actions {

struct PageActionProperties;

// Metrics may need to know the number of visible ephemeral page actions.
// This information is not available to the local instance of the metrics
// recorder, as it does not have visibility into the page action state. However,
// the `PageActionController`, which owns the metrics recorder instance, can
// determine that count. Therefore, a callback is used to retrieve the count
// from the `PageActionController`.
using VisibleEphemeralPageActionsCountCallback = base::RepeatingCallback<int()>;

// Interface for PageActionMetricsRecorder, used for concrete implementation or
// a mock for testing.
class PageActionPerActionMetricsRecorderInterface {
 public:
  PageActionPerActionMetricsRecorderInterface() = default;
  virtual ~PageActionPerActionMetricsRecorderInterface() = default;

  // Records a click event for the page action.
  virtual void RecordClick(PageActionTrigger trigger_source) = 0;
};

class PageActionPageMetricsRecorderInterface {
 public:
  PageActionPageMetricsRecorderInterface() = default;
  virtual ~PageActionPageMetricsRecorderInterface() = default;

  // Allows the page-level recorder to observe multiple page action model.
  virtual void Observe(PageActionModelInterface& model) = 0;
};

class PageActionMetricsRecorderFactory {
 public:
  virtual ~PageActionMetricsRecorderFactory() = default;
  virtual std::unique_ptr<PageActionPerActionMetricsRecorderInterface>
  CreatePerActionMetricsRecorder(
      tabs::TabInterface& tab_interface,
      const PageActionProperties& properties,
      PageActionModelInterface& model,
      VisibleEphemeralPageActionsCountCallback
          visible_ephemeral_page_actions_count_callback) = 0;

  virtual std::unique_ptr<PageActionPageMetricsRecorderInterface>
  CreatePageMetricRecorder(
      tabs::TabInterface& tab_interface,
      VisibleEphemeralPageActionsCountCallback
          visible_ephemeral_page_actions_count_callback) = 0;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_INTERFACE_H_
