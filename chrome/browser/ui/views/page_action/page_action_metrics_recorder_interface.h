// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_INTERFACE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "ui/actions/action_id.h"

namespace tabs {
class TabInterface;
}

namespace page_actions {

class PageActionModelInterface;
struct PageActionProperties;

using VisibleEphemeralPageActionsCountCallback = base::RepeatingCallback<int()>;

// Interface for PageActionMetricsRecorder, used for concrete implementation or
// a mock for testing.
class PageActionMetricsRecorderInterface {
 public:
  virtual ~PageActionMetricsRecorderInterface() = default;

  // Records a click event for the page action.
  virtual void RecordClick(actions::ActionId action_id,
                           PageActionTrigger trigger_source) = 0;

  // Allows the recorder to observe a page action model.
  virtual void Observe(PageActionModelInterface& model,
                       const PageActionProperties& properties) = 0;
};

class PageActionMetricsRecorderFactory {
 public:
  virtual ~PageActionMetricsRecorderFactory() = default;
  virtual std::unique_ptr<PageActionMetricsRecorderInterface> CreateRecorder(
      tabs::TabInterface& tab_interface,
      VisibleEphemeralPageActionsCountCallback
          visible_ephemeral_page_actions_count_callback) = 0;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_INTERFACE_H_
