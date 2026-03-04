// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_ACTOR_TASK_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_ACTOR_TASK_ICON_H_

#include "chrome/browser/ui/views/glic/glic_actor_task_icon.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class BrowserWindowInterface;

namespace glic {
class ToolbarGlicActorTaskIcon : public GlicActorTaskIcon<ToolbarButton> {
  METADATA_HEADER(ToolbarGlicActorTaskIcon, ToolbarButton)
 public:
  explicit ToolbarGlicActorTaskIcon(
      BrowserWindowInterface* browser_window_interface,
      PressedCallback pressed_callback);
  ToolbarGlicActorTaskIcon(const ToolbarGlicActorTaskIcon&) = delete;
  ToolbarGlicActorTaskIcon& operator=(const ToolbarGlicActorTaskIcon&) = delete;
  ~ToolbarGlicActorTaskIcon() override;

  void UpdateColors() override;
};
}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_GLIC_ACTOR_TASK_ICON_H_
