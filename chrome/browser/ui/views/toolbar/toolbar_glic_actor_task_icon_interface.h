// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_ACTOR_TASK_ICON_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_ACTOR_TASK_ICON_INTERFACE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "ui/views/bubble/bubble_anchor.h"

namespace glic {

// Abstract interface for controlling the toolbar Glic actor task icon (the
// autobrowse task indicator), implemented by both the Views
// (`ToolbarGlicActorTaskIcon`) and WebUI (`WebUIGlicActorTaskIconControl`)
// controls.
class ToolbarGlicActorTaskIconInterface {
 public:
  virtual void SetVisible(bool visible) = 0;
  virtual bool IsVisible() const = 0;
  virtual void SetIsShowingNudge(bool is_showing) = 0;
  virtual bool GetIsShowingNudge() const = 0;
  // Stages `nudge_label` next to the icon and uses it as the tooltip while
  // `GetIsShowingNudge()` is true.
  virtual void ShowNudgeLabel(const std::u16string& nudge_label) = 0;
  // Resets the nudge state, label, and tooltip to their defaults.
  virtual void SetTaskIconToDefault() = 0;
  // Sets whether the icon is pressed, i.e. while the task list bubble is open.
  virtual void SetPressedState(bool is_pressed) = 0;
  // Runs `callback` with the anchor for the actor task list bubble. Runs
  // synchronously if the anchor is available, otherwise once it becomes
  // available. `callback` never runs with a null anchor. An implementation
  // that waits drops `callback` without running it if the icon is hidden,
  // becomes hidden first, or a new request is made.
  virtual void GetBubbleAnchorAsync(
      base::OnceCallback<void(views::BubbleAnchor)> callback) = 0;

 protected:
  ~ToolbarGlicActorTaskIconInterface() = default;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_ACTOR_TASK_ICON_INTERFACE_H_
