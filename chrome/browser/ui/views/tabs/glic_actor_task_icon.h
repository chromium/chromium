// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_ACTOR_TASK_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_ACTOR_TASK_ICON_H_

#include <string>

#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"

namespace glic {

class GlicActorTaskIcon : public TabStripNudgeButton {
  METADATA_HEADER(GlicActorTaskIcon, TabStripNudgeButton)

 public:
  explicit GlicActorTaskIcon(TabStripController* tab_strip_controller,
                             PressedCallback pressed_callback);
  GlicActorTaskIcon(const GlicActorTaskIcon&) = delete;
  GlicActorTaskIcon& operator=(const GlicActorTaskIcon&) = delete;
  ~GlicActorTaskIcon() override;

  // TabStripControlButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // TabStripNudgeButton:
  void SetIsShowingNudge(bool is_showing) override;

  // Set the default tooltip text on the task icon.
  void SetDefaultTooltipText();

  // Sets the task icon back to its default colors.
  void SetDefaultColors();

  // Sets the task icon to its highlighted state.
  void HighlightTaskIcon();

  // Sets the task icon to the task needs assistance state.
  void ShowCheckTasksLabel();

  // Sets the task icon to its default colors, label, and tooltip text.
  void SetTaskIconToDefault();

 private:
  // Tab strip that contains this button.
  raw_ptr<TabStripController> tab_strip_controller_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_ACTOR_TASK_ICON_H_
