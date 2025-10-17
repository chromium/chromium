// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_nudge_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace tabs {
using glic::GlicInstance;
using glic::GlicKeyedService;
using glic::GlicWindowController;
using glic::Host;
using glic::mojom::CurrentView;

DEFINE_USER_DATA(GlicActorNudgeController);
GlicActorNudgeController::GlicActorNudgeController(
    BrowserWindowInterface* browser,
    TabStripActionContainer* tab_strip_action_container)
    : profile_(browser->GetProfile()),
      browser_(browser),
      tab_strip_action_container_(tab_strip_action_container),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUi)) {
    CHECK(features::kGlicActorUiNudgeRedesign.Get());
    RegisterActorNudgeStateCallback();
    UpdateCurrentActorNudgeState();
  }
}

GlicActorNudgeController::~GlicActorNudgeController() = default;

// static
GlicActorNudgeController* GlicActorNudgeController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

void GlicActorNudgeController::OnStateUpdate(
    const ActorTaskNudgeState& actor_task_nudge_state) {
  switch (actor_task_nudge_state.text) {
    case tabs::ActorTaskNudgeState::Text::kDefault:
      tab_strip_action_container_->HideGlicActorTaskIcon();
      break;
    case tabs::ActorTaskNudgeState::Text::kNeedsAttention:
      tab_strip_action_container_->TriggerGlicActorNudge(
          l10n_util::GetStringUTF16(IDR_ACTOR_CHECK_TASK_NUDGE_LABEL));
      break;
    case tabs::ActorTaskNudgeState::Text::kCompleteTasks:
      tab_strip_action_container_->TriggerGlicActorNudge(
          l10n_util::GetStringUTF16(IDR_ACTOR_TASK_COMPLETE_NUDGE_LABEL));
      break;
  }
}

void GlicActorNudgeController::RegisterActorNudgeStateCallback() {
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    actor_nudge_state_change_callback_subscription_.push_back(
        manager->RegisterTaskNudgeStateChange(base::BindRepeating(
            &GlicActorNudgeController::OnStateUpdate, base::Unretained(this))));
  }
}

void GlicActorNudgeController::UpdateCurrentActorNudgeState() {
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    OnStateUpdate(manager->GetCurrentActorTaskNudgeState());
  }
}

}  // namespace tabs
