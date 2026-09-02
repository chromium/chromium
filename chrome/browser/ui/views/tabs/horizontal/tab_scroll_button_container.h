// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_TAB_SCROLL_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_TAB_SCROLL_BUTTON_CONTAINER_H_

#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

namespace actions {
class ActionItem;
}

namespace views {
class MenuRunner;
}

// Container that holds left/right scroll buttons for the unpinned tab
// container in the horizontal tab strip. It is responsible for pagination
// style scrolling.
class TabScrollButtonContainer : public views::View,
                                 public gfx::AnimationDelegate,
                                 public views::ContextMenuController,
                                 public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(TabScrollButtonContainer, views::View)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTabScrollButtonContainer);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kStartScrollButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kEndScrollButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kUnpinMenuItem);

  explicit TabScrollButtonContainer(
      BrowserWindowInterface* browser_window_interface);
  ~TabScrollButtonContainer() override;

  bool IsPositionInWindowCaption(const gfx::Point& p);
  void SetScrollView(views::ScrollView* scroll_view);

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  class TabScrollButtonIPHController;

  struct AnimationParams {
    float start_offset;
    float target_offset;
  };

  void BeginScrollAnimation(bool scroll_to_start);

  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  actions::ActionItem* GetToggleScrollPinAction();

  // This `animation_` is used to animate the scroll view animations
  // by scrolling the `scroll_view_` with many smaller offsets
  // in `AnimationProgressed`.
  gfx::LinearAnimation animation_{this};

  std::optional<AnimationParams> animation_params_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<TabStripControlButton> start_scroll_button_ = nullptr;
  raw_ptr<TabStripControlButton> end_scroll_button_ = nullptr;

  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<TabScrollButtonIPHController> iph_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_TAB_SCROLL_BUTTON_CONTAINER_H_
