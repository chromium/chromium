// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_

#include "components/keyed_service/core/keyed_service.h"

namespace glic {
class GlicWindowController;
class Host;
}  // namespace glic

namespace actor {
class ActorKeyedService;
}  // namespace actor

class Profile;

namespace tabs {
class GlicActorTaskIconManager : public KeyedService {
 public:
  GlicActorTaskIconManager(Profile* profile,
                           actor::ActorKeyedService* actor_service,
                           glic::GlicWindowController& window_controller,
                           glic::Host& host);
  ~GlicActorTaskIconManager() override;

  // KeyedService:
  void Shutdown() override;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_ACTOR_TASK_ICON_MANAGER_H_
