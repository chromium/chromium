// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"

class BrowserWindowInterface;

namespace tabs {
class GlicActorTaskIconController {
 public:
  explicit GlicActorTaskIconController(
      BrowserWindowInterface* browser_window_interface);
  GlicActorTaskIconController(const GlicActorTaskIconController&) = delete;
  GlicActorTaskIconController& operator=(
      const GlicActorTaskIconController& other) = delete;
  virtual ~GlicActorTaskIconController();

  void OnStateUpdate();
};
}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_CONTROLLER_H_
