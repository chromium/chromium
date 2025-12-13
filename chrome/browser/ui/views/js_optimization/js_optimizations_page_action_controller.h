// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace views {
class BubbleDialogModelHost;
class View;
}  // namespace views

namespace actions {
class ActionItem;
}

// Controls the visibility of the JS optimizations omnibar icon and bubble.
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

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBubbleBodyElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBubbleButtonElementId);

  // tabs::ContentsObservingTabFeature
  void PrimaryPageChanged(content::Page& page) override;

  void ShowBubble(views::View* anchor_view, actions::ActionItem* item);

 private:
  void UpdateIconVisibility();
  void OnBubbleHidden(actions::ActionItem* action_item);
  views::BubbleDialogModelHost* CreateBubble(views::View* anchor_view,
                                             actions::ActionItem* action_item);
  void EnableV8Optimizations();

  const raw_ref<page_actions::PageActionController> page_action_controller_;
  raw_ptr<views::BubbleDialogModelHost> bubble_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_PAGE_ACTION_CONTROLLER_H_
