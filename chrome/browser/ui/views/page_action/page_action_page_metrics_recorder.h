// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PAGE_METRICS_RECORDER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PAGE_METRICS_RECORDER_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "url/gurl.h"

namespace tabs {
class TabInterface;
}

namespace page_actions {

class PageActionModelInterface;

// Records **page-level** metrics once per navigation, regardless of how many
// actions exist.
class PageActionPageMetricsRecorder
    : public PageActionPageMetricsRecorderInterface,
      public PageActionModelObserver {
 public:
  PageActionPageMetricsRecorder(
      tabs::TabInterface& tab_interface,
      base::RepeatingCallback<int()> visible_ephemeral_actions_count_cb);
  PageActionPageMetricsRecorder(const PageActionPageMetricsRecorder&) = delete;
  PageActionPageMetricsRecorder& operator=(
      const PageActionPageMetricsRecorder&) = delete;
  ~PageActionPageMetricsRecorder() override;

  // PageActionMetricsRecorderInterface:
  void Observe(PageActionModelInterface& model) override;

  // PageActionModelObserver:
  void OnPageActionModelChanged(const PageActionModelInterface& model) override;
  void OnPageActionModelWillBeDeleted(
      const PageActionModelInterface& model) override;

 private:
  void MaybeRecordPageShownMetrics();

  const raw_ref<tabs::TabInterface> tab_interface_;
  base::RepeatingCallback<int()> visible_ephemeral_actions_count_callback_;

  GURL last_committed_url_;
  bool has_recorded_action_shown_for_navigation_ = false;
  bool has_recorded_multi_shown_for_navigation_ = false;

  base::ScopedMultiSourceObservation<PageActionModelInterface,
                                     PageActionModelObserver>
      model_observations_{this};
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PAGE_METRICS_RECORDER_H_
