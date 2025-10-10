// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_ACTOR_NUDGE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_ACTOR_NUDGE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace tabs {

// Controller that handles Glic Actor notification/nudge handling.
// TODO(crbug.com/431015299): Move GlicNudgeController logic into this
// controller in order to coordinate nudge behavior between Glic and Glic Actor.
class GlicActorNudgeController {
 public:
  explicit GlicActorNudgeController(BrowserWindowInterface* browser);
  virtual ~GlicActorNudgeController();

  DECLARE_USER_DATA(GlicActorNudgeController);
  static GlicActorNudgeController* From(BrowserWindowInterface* browser);

  void OnStateUpdate();

 private:
  ::ui::ScopedUnownedUserData<GlicActorNudgeController> scoped_data_holder_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_NUDGE_CONTROLLER_H_
