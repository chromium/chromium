// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_PAGE_ACTION_CONTROLLER_H_

#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

class JsOptimizationsPageActionController
    : public tabs::ContentsObservingTabFeature {
 public:
  explicit JsOptimizationsPageActionController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller);
  ~JsOptimizationsPageActionController() override;
  JsOptimizationsPageActionController(
      const JsOptimizationsPageActionController&) = delete;
  JsOptimizationsPageActionController& operator=(
      const JsOptimizationsPageActionController&) = delete;

  // tabs::ContentsObservingTabFeature
  void PrimaryPageChanged(content::Page& page) override;

 private:
  void UpdateIconVisibility();

  const raw_ref<page_actions::PageActionController> page_action_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_PAGE_ACTION_CONTROLLER_H_
