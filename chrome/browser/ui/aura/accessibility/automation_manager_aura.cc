// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"

#include "base/no_destructor.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_root_obj_wrapper.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

// static
AutomationManagerAura* AutomationManagerAura::GetInstance() {
  static base::NoDestructor<AutomationManagerAura> instance;
  return instance.get();
}

AutomationManagerAura::AutomationManagerAura() = default;
AutomationManagerAura::~AutomationManagerAura() = default;

void AutomationManagerAura::Enable() {
  views::ViewsAXManager::Enable();

  if (!automation_event_router_observer_.IsObserving()) {
    automation_event_router_observer_.Observe(
        extensions::AutomationEventRouter::GetInstance());
  }
}

void AutomationManagerAura::Disable() {
  views::ViewsAXManager::Disable();

  if (GetTreeSource() && automation_event_router_interface_) {
    automation_event_router_interface_->DispatchTreeDestroyedEvent(
        ax_tree_id());
  }

  if (automation_event_router_observer_.IsObserving()) {
    automation_event_router_observer_.Reset();
  }
}

void AutomationManagerAura::HandleEvent(ax::mojom::Event event_type,
                                        bool from_user) {
  if (!is_enabled() || !GetTreeSource() || !GetTreeSource()->GetRoot()) {
    return;
  }

  PostEvent(GetTreeSource()->GetRoot()->GetUniqueId(), event_type,
            /*action_request_id=*/-1, from_user);
}

void AutomationManagerAura::AllAutomationExtensionsGone() {
  Disable();
}

void AutomationManagerAura::ExtensionListenerAdded() {
  if (!is_enabled()) {
    return;
  }
  Reset(/*reset_serializer=*/true);
}

void AutomationManagerAura::DispatchAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    std::vector<ui::AXTreeUpdate> tree_updates,
    const gfx::Point& mouse_location,
    std::vector<ui::AXEvent> events) {
  if (!is_enabled() || !automation_event_router_interface_) {
    return;
  }

  automation_event_router_interface_->DispatchAccessibilityEvents(
      tree_id, std::move(tree_updates), mouse_location, std::move(events));
}
