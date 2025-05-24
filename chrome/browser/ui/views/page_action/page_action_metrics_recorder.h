// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_

#include <set>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "url/gurl.h"

namespace tabs {
class TabInterface;
}

namespace page_actions {

struct PageActionProperties;

// Records visibility metrics for a specific page action, scoped to a single
// ActionId. This class does not handle all page action metrics, only for the
// one it is instantiated for.
class PageActionPerActionMetricsRecorder
    : public PageActionPerActionMetricsRecorderInterface,
      public PageActionModelObserver {
 public:
  explicit PageActionPerActionMetricsRecorder(
      tabs::TabInterface& tab_interface,
      const PageActionProperties& properties,
      PageActionModelInterface& model,
      VisibleEphemeralPageActionsCountCallback
          visible_ephemeral_page_actions_count_callback);

  PageActionPerActionMetricsRecorder(
      const PageActionPerActionMetricsRecorder&) = delete;
  PageActionPerActionMetricsRecorder operator=(
      const PageActionPerActionMetricsRecorder&) = delete;

  ~PageActionPerActionMetricsRecorder() override;

  // PageActionPerActionMetricsRecorderInterface:
  void RecordClick(PageActionTrigger trigger_source) override;

  // PageActionModelObserver
  void OnPageActionModelChanged(const PageActionModelInterface& model) override;
  void OnPageActionModelWillBeDeleted(
      const PageActionModelInterface& model) override;

 private:
  void OnPageActionVisible();

  // Tracks if the icon's "Shown" metric has been recorded for the URL in the
  // set.
  std::set<GURL> page_action_recorded_urls_;

  // Properties associated with the specific page action being observed.
  PageActionIconType page_action_type_;
  std::string histogram_name_;

  // Used to get count of visible ephemeral page actions from the
  // `PageActionController`.
  VisibleEphemeralPageActionsCountCallback
      visible_ephemeral_page_actions_count_callback_;

  // The TabInterface is guaranteed valid for this object’s lifetime.
  const raw_ref<tabs::TabInterface> tab_interface_;

  // Tracks per-navigation state used for metric recording.
  GURL last_committed_url_;

  base::ScopedObservation<PageActionModelInterface, PageActionModelObserver>
      scoped_observation_{this};
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_
