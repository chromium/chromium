// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_PAGE_ACTION_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace views {
class BubbleDialogModelHost;
}  // namespace views

namespace actions {
class ActionItem;
}

// Controls the visibility of the JS optimizations omnibar icon and bubble.
class JsOptimizationsPageActionController
    : public tabs::ContentsObservingTabFeature,
      public views::WidgetObserver {
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

  using BubbleCreatedCallback =
      base::RepeatingCallback<void(views::BubbleDialogModelHost*)>;
  static void SetBubbleCreatedCallbackForTesting(
      BubbleCreatedCallback callback);

  // tabs::ContentsObservingTabFeature
  void PrimaryPageChanged(content::Page& page) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

  void ShowBubble(views::BubbleAnchor anchor, actions::ActionItem* item);

 private:
  void UpdateIconVisibility();
  void OnBubbleHidden(actions::ActionItem* action_item);
  views::BubbleDialogModelHost* CreateBubble(views::BubbleAnchor anchor,
                                             actions::ActionItem* action_item);
  void EnableV8Optimizations();

  const raw_ref<page_actions::PageActionController> page_action_controller_;
  raw_ptr<views::BubbleDialogModelHost> bubble_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::WeakPtrFactory<JsOptimizationsPageActionController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_JS_OPTIMIZATION_JS_OPTIMIZATIONS_PAGE_ACTION_CONTROLLER_H_
