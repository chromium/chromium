// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_ACTOR_TASK_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_ACTOR_TASK_ICON_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/views/glic/glic_actor_task_icon.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "ui/views/controls/button/menu_button_controller.h"

class BrowserWindowInterface;

namespace glic {

class TabStripGlicActorTaskIcon
    : public GlicActorTaskIcon<TabStripNudgeButton> {
  METADATA_HEADER(TabStripGlicActorTaskIcon, TabStripNudgeButton)

 public:
  explicit TabStripGlicActorTaskIcon(
      BrowserWindowInterface* browser_window_interface,
      PressedCallback pressed_callback);
  TabStripGlicActorTaskIcon(const TabStripGlicActorTaskIcon&) = delete;
  TabStripGlicActorTaskIcon& operator=(const TabStripGlicActorTaskIcon&) =
      delete;
  ~TabStripGlicActorTaskIcon() override;

  // TabStripNudgeButton:
  bool GetIsShowingNudge() const override;
  void SetIsShowingNudge(bool is_showing) override;

  // Sets the task icon back to its default colors.
  void SetDefaultColors() override;

  // GetBoundsInScreen() gives a rect with some padding that extends beyond the
  // visible edges of the button. This function returns a rect without that
  // padding in order to anchor the ActorTaskListBubble on the edge of the
  // button.
  gfx::Rect GetAnchorBoundsInScreen() const override;

 private:
  void NotifyClick(const ui::Event& event) override;

  // Whether or not to use the same background as the alt icon, which may be
  // enabled as part of GlicEntrypointVariations. Should be kept in sync with
  // TapStripGlicButton::ShouldUseAltIcon.
  bool ShouldUseGlicButtonAltIconBackgroundColor();

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_TAB_STRIP_GLIC_ACTOR_TASK_ICON_H_
