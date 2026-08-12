// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_TAB_SCROLL_BUTTON_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_TAB_SCROLL_BUTTON_CONTAINER_H_

#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

// Container that holds left/right scroll buttons for the unpinned tab
// container in the horizontal tab strip. It is responsible for pagination
// style scrolling.
class TabScrollButtonContainer : public views::View,
                                 public gfx::AnimationDelegate {
  METADATA_HEADER(TabScrollButtonContainer, views::View)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTabScrollButtonContainer);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kStartScrollButton);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kEndScrollButton);

  explicit TabScrollButtonContainer(
      BrowserWindowInterface* browser_window_interface);
  bool IsPositionInWindowCaption(const gfx::Point& p);
  void SetScrollView(views::ScrollView* scroll_view);

 private:
  struct AnimationParams {
    bool scroll_to_start;
    int amount_to_scroll;
    float last_progress = 0;
  };

  void BeginScrollAnimation(bool scroll_to_start);

  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // This `animation_` is used to animate the scroll view animations
  // by scrolling the `scroll_view_` with many smaller offsets
  // in `AnimationProgressed`.
  gfx::LinearAnimation animation_{this};

  std::optional<AnimationParams> animation_params_;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  raw_ptr<TabStripControlButton> start_scroll_button_ = nullptr;
  raw_ptr<TabStripControlButton> end_scroll_button_ = nullptr;
};
#endif  // CHROME_BROWSER_UI_VIEWS_TABS_HORIZONTAL_TAB_SCROLL_BUTTON_CONTAINER_H_
