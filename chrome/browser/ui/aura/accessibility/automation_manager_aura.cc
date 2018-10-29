// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"

#include <stddef.h>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_id_registry.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/chromeos/accessibility/ax_host_service.h"
#include "ui/base/ui_base_features.h"
#endif

using extensions::AutomationEventRouter;

// static
AutomationManagerAura* AutomationManagerAura::GetInstance() {
  return base::Singleton<AutomationManagerAura>::get();
}

void AutomationManagerAura::Enable() {
  enabled_ = true;
  Reset(false);

  SendEvent(current_tree_->GetRoot(), ax::mojom::Event::kLoadComplete);
  views::AXAuraObjCache::GetInstance()->SetDelegate(this);

#if defined(OS_CHROMEOS)
  // TODO(crbug.com/756054): Support SingleProcessMash and MultiProcessMash.
  if (!features::IsUsingWindowService()) {
    aura::Window* active_window = ash::wm::GetActiveWindow();
    if (active_window) {
      views::AXAuraObjWrapper* focus =
          views::AXAuraObjCache::GetInstance()->GetOrCreate(active_window);
      if (focus)
        SendEvent(focus, ax::mojom::Event::kChildrenChanged);
    }
  }
  // Gain access to out-of-process native windows.
  AXHostService::SetAutomationEnabled(true);
#endif
}

void AutomationManagerAura::Disable() {
  enabled_ = false;
  Reset(true);

#if defined(OS_CHROMEOS)
  AXHostService::SetAutomationEnabled(false);
#endif
}

void AutomationManagerAura::HandleEvent(views::View* view,
                                        ax::mojom::Event event_type) {
  CHECK(view);

  if (!enabled_)
    return;

  views::AXAuraObjWrapper* obj =
      views::AXAuraObjCache::GetInstance()->GetOrCreate(view);
  if (!obj)
    return;

  // Post a task to handle the event at the end of the current call stack.
  // This helps us avoid firing accessibility events for transient changes.
  // because there's a chance that the underlying object being wrapped could
  // be deleted, pass the ID of the object rather than the object pointer.
  int32_t id = obj->GetUniqueId().Get();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&AutomationManagerAura::SendEventOnObjectById,
                     weak_ptr_factory_.GetWeakPtr(), id, event_type));
}

void AutomationManagerAura::HandleEvent(ax::mojom::Event event_type) {
  views::AXAuraObjWrapper* obj = current_tree_->GetRoot();
  if (!obj)
    return;

  AutomationManagerAura::SendEvent(obj, event_type);
}

void AutomationManagerAura::SendEventOnObjectById(int32_t id,
                                                  ax::mojom::Event event_type) {
  views::AXAuraObjWrapper* obj = views::AXAuraObjCache::GetInstance()->Get(id);
  if (obj)
    SendEvent(obj, event_type);
}

void AutomationManagerAura::HandleAlert(const std::string& text) {
  if (!enabled_)
    return;

  views::AXAuraObjWrapper* obj =
      static_cast<AXRootObjWrapper*>(current_tree_->GetRoot())
          ->GetAlertForText(text);
  SendEvent(obj, ax::mojom::Event::kAlert);
}

void AutomationManagerAura::PerformAction(const ui::AXActionData& data) {
  CHECK(enabled_);

  // Unlike all of the other actions, a hit test requires determining the
  // node to perform the action on first.
  if (data.action == ax::mojom::Action::kHitTest) {
    PerformHitTest(data);
    return;
  }

  current_tree_->HandleAccessibleAction(data);
}

void AutomationManagerAura::OnChildWindowRemoved(
    views::AXAuraObjWrapper* parent) {
  if (!enabled_)
    return;

  if (!parent)
    parent = current_tree_->GetRoot();

  SendEvent(parent, ax::mojom::Event::kChildrenChanged);
}

void AutomationManagerAura::OnEvent(views::AXAuraObjWrapper* aura_obj,
                                    ax::mojom::Event event_type) {
  SendEvent(aura_obj, event_type);
}

AutomationManagerAura::AutomationManagerAura()
    : AXHostDelegate(ui::DesktopAXTreeID()),
      enabled_(false),
      processing_events_(false),
      weak_ptr_factory_(this) {}

AutomationManagerAura::~AutomationManagerAura() {
}

void AutomationManagerAura::Reset(bool reset_serializer) {
  if (!current_tree_)
    current_tree_.reset(new AXTreeSourceAura());
  reset_serializer ? current_tree_serializer_.reset()
                   : current_tree_serializer_.reset(
                         new AuraAXTreeSerializer(current_tree_.get()));
}

void AutomationManagerAura::SendEvent(views::AXAuraObjWrapper* aura_obj,
                                      ax::mojom::Event event_type) {
  if (!enabled_)
    return;

  if (!current_tree_serializer_)
    return;

  if (processing_events_) {
    pending_events_.push_back(std::make_pair(aura_obj, event_type));
    return;
  }
  processing_events_ = true;

  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = ui::DesktopAXTreeID();
  event_bundle.mouse_location = aura::Env::GetInstance()->last_mouse_location();

  ui::AXTreeUpdate update;
  if (!current_tree_serializer_->SerializeChanges(aura_obj, &update)) {
    LOG(ERROR) << "Unable to serialize one accessibility event.";
    return;
  }
  event_bundle.updates.push_back(update);

  // Make sure the focused node is serialized.
  views::AXAuraObjWrapper* focus =
      views::AXAuraObjCache::GetInstance()->GetFocus();
  if (focus) {
    ui::AXTreeUpdate focused_node_update;
    current_tree_serializer_->SerializeChanges(focus, &focused_node_update);
    event_bundle.updates.push_back(focused_node_update);
  }

  // Fire the event on the node, but only if it's actually in the tree.
  // Sometimes we get events fired on nodes with an ancestor that's
  // marked invisible, for example. In those cases we should still
  // call SerializeChanges (because the change may have affected the
  // ancestor) but we shouldn't fire the event on the node not in the tree.
  if (current_tree_serializer_->IsInClientTree(aura_obj)) {
    ui::AXEvent event;
    event.id = aura_obj->GetUniqueId().Get();
    event.event_type = event_type;
    event_bundle.events.push_back(event);
  }

  AutomationEventRouter* router = AutomationEventRouter::GetInstance();
  router->DispatchAccessibilityEvents(event_bundle);

  if (event_bundle_callback_for_testing_)
    event_bundle_callback_for_testing_.Run(event_bundle);

  processing_events_ = false;
  auto pending_events_copy = pending_events_;
  pending_events_.clear();
  for (size_t i = 0; i < pending_events_copy.size(); ++i) {
    SendEvent(pending_events_copy[i].first, pending_events_copy[i].second);
  }
}

void AutomationManagerAura::PerformHitTest(
    const ui::AXActionData& original_action) {
#if defined(OS_CHROMEOS)
  ui::AXActionData action = original_action;
  aura::Window* root_window = ash::Shell::Get()->GetPrimaryRootWindow();
  if (!root_window)
    return;

  // Determine which aura Window is associated with the target point.
  aura::Window* window =
      root_window->GetEventHandlerForPoint(action.target_point);
  if (!window)
    return;

  // Convert point to local coordinates of the hit window.
  aura::Window::ConvertPointToTarget(root_window, window, &action.target_point);

  // Check for a AX node tree in a remote process (e.g. renderer, mojo app).
  ui::AXTreeID child_ax_tree_id;
  std::string* child_ax_tree_id_ptr = window->GetProperty(ui::kChildAXTreeID);
  if (child_ax_tree_id_ptr)
    child_ax_tree_id = ui::AXTreeID::FromString(*child_ax_tree_id_ptr);

  // If the window has a child AX tree ID, forward the action to the
  // associated AXHostDelegate or RenderFrameHost.
  if (child_ax_tree_id != ui::AXTreeIDUnknown()) {
    ui::AXTreeIDRegistry* registry = ui::AXTreeIDRegistry::GetInstance();
    ui::AXHostDelegate* delegate = registry->GetHostDelegate(child_ax_tree_id);
    if (delegate) {
      delegate->PerformAction(action);
      return;
    }

    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromAXTreeID(child_ax_tree_id);
    if (rfh) {
      // Convert to pixels for the RenderFrameHost HitTest.
      window->GetHost()->ConvertDIPToPixels(&action.target_point);
      rfh->AccessibilityPerformAction(action);
    }
    return;
  }

  // If the window doesn't have a child tree ID, try to fire the event
  // on a View.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (widget) {
    views::View* root_view = widget->GetRootView();
    views::View* hit_view =
        root_view->GetEventHandlerForPoint(action.target_point);
    if (hit_view) {
      hit_view->NotifyAccessibilityEvent(action.hit_test_event_to_fire, true);
      return;
    }
  }

  // Otherwise, fire the event directly on the Window.
  views::AXAuraObjWrapper* window_wrapper =
      views::AXAuraObjCache::GetInstance()->GetOrCreate(window);
  if (window_wrapper)
    SendEvent(window_wrapper, action.hit_test_event_to_fire);
#endif
}
