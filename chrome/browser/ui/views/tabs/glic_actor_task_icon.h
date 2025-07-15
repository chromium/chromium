// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GLIC_ACTOR_TASK_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GLIC_ACTOR_TASK_ICON_H_

#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "ui/gfx/vector_icon_types.h"

namespace glic {

class GlicActorTaskIcon : public TabStripNudgeButton {
  METADATA_HEADER(GlicActorTaskIcon, TabStripNudgeButton)

 public:
  explicit GlicActorTaskIcon(TabStripController* tab_strip_controller,
                             const gfx::VectorIcon& icon,
                             const std::u16string& tooltip);
  GlicActorTaskIcon(const GlicActorTaskIcon&) = delete;
  GlicActorTaskIcon& operator=(const GlicActorTaskIcon&) = delete;
  ~GlicActorTaskIcon() override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GLIC_ACTOR_TASK_ICON_H_
