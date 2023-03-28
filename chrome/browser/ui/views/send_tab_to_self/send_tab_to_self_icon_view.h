// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ICON_VIEW_H_

#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class CommandUpdater;

namespace send_tab_to_self {

class SendTabToSelfBubbleController;

// The location bar icon to show the send tab to self bubble where the user can
// choose to share the url to a target device.
class SendTabToSelfIconView : public PageActionIconView {
 public:
  METADATA_HEADER(SendTabToSelfIconView);
  SendTabToSelfIconView(
      CommandUpdater* command_updater,
      IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
      PageActionIconView::Delegate* page_action_icon_delegate);
  SendTabToSelfIconView(const SendTabToSelfIconView&) = delete;
  SendTabToSelfIconView& operator=(const SendTabToSelfIconView&) = delete;
  ~SendTabToSelfIconView() override;

  // PageActionIconView:
  views::BubbleDialogDelegate* GetBubble() const override;
  void UpdateImpl() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 protected:
  // PageActionIconView:
  void OnExecuting(PageActionIconView::ExecuteSource execute_source) override;
  const gfx::VectorIcon& GetVectorIcon() const override;

  // Updates the opacity according to the length of the label view as it is
  // shrinking.
  void UpdateOpacity();

 private:
  enum class AnimationState { kNotShown, kShowing, kShown };

  SendTabToSelfBubbleController* GetController() const;

  // Indicates the current state of the initial "Send" animation.
  AnimationState initial_animation_state_ = AnimationState::kNotShown;
  // Indicates whether the "Sending..." animation has been shown since the last
  // time the omnibox was in focus.
  AnimationState sending_animation_state_ = AnimationState::kNotShown;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_VIEWS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ICON_VIEW_H_
