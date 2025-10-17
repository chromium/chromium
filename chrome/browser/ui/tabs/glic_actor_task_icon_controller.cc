// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/glic_actor_task_icon_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
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

DEFINE_USER_DATA(GlicActorTaskIconController);
GlicActorTaskIconController::GlicActorTaskIconController(
    BrowserWindowInterface* browser,
    TabStripActionContainer* tab_strip_action_container)
    : profile_(browser->GetProfile()),
      browser_(browser),
      tab_strip_action_container_(tab_strip_action_container),
      scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {
  if (base::FeatureList::IsEnabled(features::kGlicActorUi)) {
    RegisterTaskIconStateCallback();
    UpdateCurrentTaskIconUiState();
  }
}

GlicActorTaskIconController::~GlicActorTaskIconController() = default;

// static
GlicActorTaskIconController* GlicActorTaskIconController::From(
    BrowserWindowInterface* browser) {
  return ui::ScopedUnownedUserData<GlicActorTaskIconController>::Get(
      browser->GetUnownedUserDataHost());
}

void GlicActorTaskIconController::RegisterTaskIconStateCallback() {
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    task_icon_state_change_callback_subscription_.push_back(
        manager->RegisterTaskIconStateChange(
            base::BindRepeating(&GlicActorTaskIconController::OnStateUpdate,
                                base::Unretained(this))));
  }
}

void GlicActorTaskIconController::UpdateCurrentTaskIconUiState() {
  if (auto* manager =
          GlicActorTaskIconManagerFactory::GetForProfile(profile_)) {
    auto* glic_service = GlicKeyedService::Get(profile_);

    // TODO(crbug.com/446734119): Instead ActorTask should hold a glic
    // InstanceId and use that to retrieve the instance.
    GlicInstance* instance = glic_service->GetInstanceForActiveTab(browser_);
    if (!instance) {
      return;
    }

    OnStateUpdate(instance->IsShowing(),
                  instance->host().GetPrimaryCurrentView(),
                  manager->GetCurrentActorTaskIconState());
  }
}

void GlicActorTaskIconController::OnStateUpdate(
    bool is_showing,
    CurrentView current_view,
    const ActorTaskIconState& actor_task_icon_state) {
  // If the task icon is inactive, hide it and perform no additional style
  // changes.
  if (!actor_task_icon_state.is_visible) {
    tab_strip_action_container_->HideGlicActorTaskIcon();
    return;
  }

  // Determines the text to be shown.
  switch (actor_task_icon_state.text) {
    case ActorTaskIconState::Text::kDefault:
      tab_strip_action_container_->ShowGlicActorTaskIcon();
      break;
    case ActorTaskIconState::Text::kNeedsAttention:
      tab_strip_action_container_->TriggerGlicActorNudge(
          l10n_util::GetStringUTF16(IDR_ACTOR_CHECK_TASK_NUDGE_LABEL));
      break;
    case ActorTaskIconState::Text::kCompleteTasks:
      tab_strip_action_container_->TriggerGlicActorNudge(
          l10n_util::GetStringUTF16(IDR_ACTOR_TASK_COMPLETE_NUDGE_LABEL));
      break;
  }

  // Determines highlight + tooltip styling.
  if (is_showing) {
    tab_strip_action_container_->glic_actor_task_icon()
        ->SetFloatyOpenTooltipText();
  } else {
    tab_strip_action_container_->glic_actor_task_icon()
        ->SetFloatyClosedTooltipText();
  }
}

}  // namespace tabs
