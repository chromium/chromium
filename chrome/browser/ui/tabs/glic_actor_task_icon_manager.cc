// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "glic_actor_task_icon_manager.h"

namespace tabs {
using actor::ActorKeyedService;
using glic::GlicWindowController;
using glic::Host;

GlicActorTaskIconManager::GlicActorTaskIconManager(
    Profile* profile,
    ActorKeyedService* actor_service,
    GlicWindowController& window_controller,
    Host& host) {}

GlicActorTaskIconManager::~GlicActorTaskIconManager() = default;

void GlicActorTaskIconManager::Shutdown() {}

}  // namespace tabs
