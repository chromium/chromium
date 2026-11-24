// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_GLIC_ACTOR_TASK_ICON_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_GLIC_ACTOR_TASK_ICON_CONTROL_H_

#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_actor_task_icon_interface.h"
#include "ui/views/bubble/bubble_anchor.h"

class WebUIToolbarControlDelegate;

namespace ui {
class TrackedElement;
}  // namespace ui

// Manages the state of the WebUI Glic actor task icon (the autobrowse task
// indicator), which is the WebUI counterpart of glic::ToolbarGlicActorTaskIcon.
// The icon is shown next to the Glic button while an actor task is active; the
// two are styled as a split button.
class WebUIGlicActorTaskIconControl final
    : public glic::ToolbarGlicActorTaskIconInterface {
 public:
  explicit WebUIGlicActorTaskIconControl(WebUIToolbarControlDelegate* delegate);
  WebUIGlicActorTaskIconControl(const WebUIGlicActorTaskIconControl&) = delete;
  WebUIGlicActorTaskIconControl& operator=(
      const WebUIGlicActorTaskIconControl&) = delete;
  ~WebUIGlicActorTaskIconControl();

  void Init();

  // Called when the WebUI icon is clicked. Shows the actor task list bubble if
  // not already pressed, matching ToolbarView::OnGlicSecondaryButtonClicked().
  void OnClicked();

  // Whether the icon is allowed to show. The icon is half of the Glic split
  // button, so this is false while the Glic button is hidden, mirroring
  // ToolbarView::UpdateGlicButtonVisibility() hiding the Views
  // `glic_actor_button_container_`. The icon shows only if both IsVisible()
  // and this are true.
  void SetCanShow(bool can_show);

  // Whether the icon is shown in the toolbar, i.e. both IsVisible() and the
  // value passed to SetCanShow() are true.
  bool ShouldShow() const { return is_visible_ && can_show_; }

  bool GetIsPressed() const { return is_pressed_; }

  // glic::ToolbarGlicActorTaskIconInterface:
  void SetVisible(bool visible) override;
  bool IsVisible() const override;
  void ShowNudgeLabel(const std::u16string& nudge_label) override;
  void SetTaskIconToDefault() override;
  void SetIsShowingNudge(bool is_showing) override;
  bool GetIsShowingNudge() const override;
  void SetPressedState(bool is_pressed) override;
  // Waits for the actor task icon's TrackedElement when the bubble is
  // requested in the same step that shows the icon, before the WebUI has
  // reported the element's bounds.
  void GetBubbleAnchorAsync(
      base::OnceCallback<void(views::BubbleAnchor)> callback) override;

 private:
  void UpdateState();
  void OnAnchorElementShown(ui::TrackedElement* element);
  void CancelPendingBubbleAnchorRequest();

  const raw_ptr<WebUIToolbarControlDelegate> delegate_;
  bool is_initialized_ = false;
  bool is_visible_ = false;
  bool can_show_ = true;
  bool is_showing_nudge_ = false;
  bool is_pressed_ = false;
  std::u16string nudge_label_;
  base::OnceCallback<void(views::BubbleAnchor)> pending_bubble_anchor_callback_;
  base::CallbackListSubscription anchor_element_shown_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_GLIC_ACTOR_TASK_ICON_CONTROL_H_
