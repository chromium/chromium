// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_

#include <set>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "url/gurl.h"

namespace tabs {
class TabInterface;
}

namespace page_actions {

struct PageActionProperties;

// Interface for PageActionMetricsRecorder, used for concrete implementation or
// a mock for testing.
class PageActionMetricsRecorderInterface {
 public:
  PageActionMetricsRecorderInterface() = default;
  virtual ~PageActionMetricsRecorderInterface() = default;
};

class PageActionMetricsRecorderFactory {
 public:
  virtual ~PageActionMetricsRecorderFactory() = default;
  virtual std::unique_ptr<PageActionMetricsRecorderInterface> Create(
      tabs::TabInterface& tab_interface,
      const PageActionProperties& properties,
      PageActionModelInterface& model) = 0;
};

// Records visibility metrics for a specific page action, scoped to a single
// ActionId. This class does not handle all page action metrics, only for the
// one it is instantiated for.
class PageActionMetricsRecorder : public PageActionMetricsRecorderInterface,
                                  public PageActionModelObserver {
 public:
  explicit PageActionMetricsRecorder(tabs::TabInterface& tab_interface,
                                     const PageActionProperties& properties,
                                     PageActionModelInterface& model);

  PageActionMetricsRecorder(const PageActionMetricsRecorder&) = delete;
  PageActionMetricsRecorder operator=(const PageActionMetricsRecorder&) =
      delete;

  ~PageActionMetricsRecorder() override;

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
  bool is_ephemeral_;
  PageActionIconType page_action_type_;

  // The TabInterface is guaranteed valid for this object’s lifetime.
  const raw_ref<tabs::TabInterface> tab_interface_;

  base::ScopedObservation<PageActionModelInterface, PageActionModelObserver>
      scoped_observation_{this};
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_METRICS_RECORDER_H_
