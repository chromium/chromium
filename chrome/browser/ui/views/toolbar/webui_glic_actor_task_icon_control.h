// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_GLIC_ACTOR_TASK_ICON_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_GLIC_ACTOR_TASK_ICON_CONTROL_H_

#include <string>

#include "base/memory/raw_ptr.h"

class WebUIToolbarControlDelegate;

// Manages the state of the WebUI Glic actor task icon (the autobrowse task
// indicator), which is the WebUI counterpart of glic::ToolbarGlicActorTaskIcon.
// The icon is shown next to the Glic button while an actor task is active; the
// two are styled as a split button.
class WebUIGlicActorTaskIconControl {
 public:
  explicit WebUIGlicActorTaskIconControl(WebUIToolbarControlDelegate* delegate);
  WebUIGlicActorTaskIconControl(const WebUIGlicActorTaskIconControl&) = delete;
  WebUIGlicActorTaskIconControl& operator=(
      const WebUIGlicActorTaskIconControl&) = delete;
  ~WebUIGlicActorTaskIconControl();

  void Init();

  // Called when the WebUI icon is clicked. Shows the actor task list bubble if
  // not already pressed, matching ToolbarView::OnGlicActorTaskIconClicked().
  void OnClicked();

  void SetVisible(bool visible);
  bool IsVisible() const { return is_visible_; }

  // Whether the icon is allowed to show. The icon is half of the Glic split
  // button, so this is false while the Glic button is hidden, mirroring
  // ToolbarView::UpdateGlicButtonVisibility() hiding the Views
  // `glic_actor_button_container_`. The icon shows only if both IsVisible()
  // and this are true.
  void SetCanShow(bool can_show);

  // Whether the icon is shown in the toolbar, i.e. both IsVisible() and the
  // value passed to SetCanShow() are true.
  bool ShouldShow() const { return is_visible_ && can_show_; }

  // Stages `nudge_label` next to the icon and uses it as the tooltip while
  // `GetIsShowingNudge()` is true.
  void ShowNudgeLabel(const std::u16string& nudge_label);

  // Resets the nudge state, label, and tooltip to their defaults.
  void SetTaskIconToDefault();

  void SetIsShowingNudge(bool is_showing);
  bool GetIsShowingNudge() const {
    return is_showing_nudge_ && !nudge_label_.empty();
  }

  // Sets whether the icon is pressed, i.e. while the task list bubble is open.
  void SetPressedState(bool is_pressed);
  bool GetIsPressed() const { return is_pressed_; }

 private:
  void UpdateState();

  const raw_ptr<WebUIToolbarControlDelegate> delegate_;
  bool is_initialized_ = false;
  bool is_visible_ = false;
  bool can_show_ = true;
  bool is_showing_nudge_ = false;
  bool is_pressed_ = false;
  std::u16string nudge_label_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_GLIC_ACTOR_TASK_ICON_CONTROL_H_
