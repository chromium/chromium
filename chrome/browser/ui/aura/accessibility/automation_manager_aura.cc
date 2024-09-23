// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_action_handler_base.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/accessibility_alert_window.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_root_obj_wrapper.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

// static
AutomationManagerAura* AutomationManagerAura::GetInstance() {
  static base::NoDestructor<AutomationManagerAura> instance;
  return instance.get();
}

void AutomationManagerAura::Enable() {
  enabled_ = true;
  Reset(false);

  // Seed the views::AXAuraObjCache with per-display root windows so
  // GetTopLevelWindows() returns the correct values when automation is enabled
  // with multiple displays connected.
  if (send_window_state_on_enable_) {
    for (aura::WindowTreeHost* host :
         aura::Env::GetInstance()->window_tree_hosts()) {
      cache_->OnRootWindowObjCreated(host->window());
    }
  }

  // Send this event immediately to push the initial desktop tree state.
  pending_events_.push_back({tree_->GetRoot()->GetUniqueId(),
                             ax::mojom::Event::kLoadComplete, -1,
                             currently_performing_action_});
  SendPendingEvents();
  // Intentionally not reset at shutdown since we cannot rely on the shutdown
  // ordering of two base::Singletons.
  cache_->SetDelegate(this);

  const display::Display& display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  aura::Window* root_window = nullptr;
  for (aura::WindowTreeHost* host :
       aura::Env::GetInstance()->window_tree_hosts()) {
    if (display.id() == host->GetDisplayId()) {
      root_window = host->window();
      break;
    }
  }

  aura::Window* active_window = nullptr;
  if (root_window) {
    active_window = ::wm::GetActivationClient(root_window)->GetActiveWindow();
  }

  if (active_window) {
    views::AXAuraObjWrapper* focus = cache_->GetOrCreate(active_window);
    if (focus)
      PostEvent(focus->GetUniqueId(), ax::mojom::Event::kChildrenChanged);
  }

  if (!automation_event_router_observer_.IsObserving()) {
    automation_event_router_observer_.Observe(
        extensions::AutomationEventRouter::GetInstance());
  }
}

void AutomationManagerAura::Disable() {
  enabled_ = false;
  if (tree_) {
    if (automation_event_router_interface_)
      automation_event_router_interface_->DispatchTreeDestroyedEvent(
          tree_->tree_id());
    tree_.reset();
  }
  tree_serializer_.reset();
  alert_window_.reset();
  cache_ = std::make_unique<views::AXAuraObjCache>();

  if (automation_event_router_observer_.IsObserving())
    automation_event_router_observer_.Reset();
}

void AutomationManagerAura::OnViewEvent(views::View* view,
                                        ax::mojom::Event event_type) {
  CHECK(view);

  if (!enabled_)
    return;

  DCHECK(tree_.get());

  views::AXAuraObjWrapper* obj = cache_->GetOrCreate(view);
  if (!obj)
    return;

  PostEvent(obj->GetUniqueId(), event_type);
}

void AutomationManagerAura::OnVirtualViewEvent(
    views::AXVirtualView* virtual_view,
    ax::mojom::Event event_type) {
  CHECK(virtual_view);

  if (!enabled_)
    return;

  DCHECK(tree_.get());

  views::AXAuraObjWrapper* obj = virtual_view->GetOrCreateWrapper(cache_.get());
  if (!obj)
    return;

  PostEvent(obj->GetUniqueId(), event_type);
}

void AutomationManagerAura::AllAutomationExtensionsGone() {
  Disable();
}

void AutomationManagerAura::ExtensionListenerAdded() {
  if (!enabled_) {
    return;
  }

  Reset(true /* reset serializer */);
}

void AutomationManagerAura::HandleEvent(ax::mojom::Event event_type,
                                        bool from_user) {
  if (!enabled_)
    return;

  DCHECK(tree_.get());
  views::AXAuraObjWrapper* obj = tree_->GetRoot();
  if (!obj)
    return;

  PostEvent(obj->GetUniqueId(), event_type, /*action_request_id=*/-1,
            /*from_user=*/from_user);
}

void AutomationManagerAura::HandleAlert(const std::string& text) {
  if (!enabled_)
    return;

  DCHECK(tree_.get());
  if (alert_window_.get())
    alert_window_->HandleAlert(text);
}

void AutomationManagerAura::PerformAction(const ui::AXActionData& data) {
  if (!enabled_)
    return;

  DCHECK(tree_.get());

  base::AutoReset<ax::mojom::Action> reset_currently_performing_action(
      &currently_performing_action_, data.action);

  // Exclude the do default action, which can trigger too many important events
  // that should not be ignored by clients like focus.
  if (data.action == ax::mojom::Action::kDoDefault)
    currently_performing_action_ = ax::mojom::Action::kNone;

  // Unlike all of the other actions, a hit test requires determining the
  // node to perform the action on first.
  if (data.action == ax::mojom::Action::kHitTest) {
    PerformHitTest(data);
    return;
  }

  tree_->HandleAccessibleAction(data);
}

void AutomationManagerAura::SetA11yOverrideWindow(
    aura::Window* a11y_override_window) {
  cache_->SetA11yOverrideWindow(a11y_override_window);
}

void AutomationManagerAura::OnChildWindowRemoved(
    views::AXAuraObjWrapper* parent) {
  if (!enabled_)
    return;

  DCHECK(tree_.get());

  if (!parent)
    parent = tree_->GetRoot();

  PostEvent(parent->GetUniqueId(), ax::mojom::Event::kChildrenChanged);
}

void AutomationManagerAura::OnEvent(views::AXAuraObjWrapper* aura_obj,
                                    ax::mojom::Event event_type) {
  if (!enabled_) {
    return;
  }

  PostEvent(aura_obj->GetUniqueId(), event_type);
}

AutomationManagerAura::AutomationManagerAura()
    : cache_(std::make_unique<views::AXAuraObjCache>()) {
  views::AXEventManager::Get()->AddObserver(this);
}

// Never runs because object is leaked.
AutomationManagerAura::~AutomationManagerAura() = default;

void AutomationManagerAura::Reset(bool reset_serializer) {
  if (!tree_) {
    auto desktop_root = std::make_unique<AXRootObjWrapper>(this, cache_.get());
    tree_ = std::make_unique<views::AXTreeSourceViews>(
        desktop_root->GetUniqueId(), ax_tree_id(), cache_.get());
    cache_->CreateOrReplace(std::move(desktop_root));
  }
  if (reset_serializer) {
    tree_serializer_.reset();
    alert_window_.reset();
  } else {
    tree_serializer_ = std::make_unique<AuraAXTreeSerializer>(tree_.get());

    const auto& hosts = aura::Env::GetInstance()->window_tree_hosts();
    if (!hosts.empty()) {
      alert_window_ = std::make_unique<views::AccessibilityAlertWindow>(
          hosts[0]->window(), cache_.get());
    }
  }
}

void AutomationManagerAura::PostEvent(int id,
                                      ax::mojom::Event event_type,
                                      int action_request_id,
                                      bool from_user) {
  pending_events_.push_back({id, event_type, action_request_id,
                             currently_performing_action_, from_user});

  if (processing_posted_)
    return;

  processing_posted_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AutomationManagerAura::SendPendingEvents,
                                base::Unretained(this)));
}

void AutomationManagerAura::SendPendingEvents() {
  processing_posted_ = false;
  if (!enabled_)
    return;

  if (!tree_serializer_)
    return;

  std::vector<ui::AXTreeUpdate> tree_updates;
  std::vector<ui::AXEvent> events;
  auto pending_events_copy = std::move(pending_events_);
  pending_events_.clear();
  for (auto& event_copy : pending_events_copy) {
    const int id = event_copy.id;
    const ax::mojom::Event event_type = event_copy.event_type;
    auto* aura_obj = cache_->Get(id);

    // Some events are important enough where even if their ax obj was
    // destroyed, they still need to be fired.
    if (event_type == ax::mojom::Event::kMenuEnd && !aura_obj)
      aura_obj = tree_->GetRoot();

    if (!aura_obj)
      continue;

    ui::AXTreeUpdate update;
    if (!tree_serializer_->SerializeChanges(aura_obj, &update)) {
      OnSerializeFailure(event_type, update);
      return;
    }
    tree_updates.push_back(std::move(update));

    // Fire the event on the node, but only if it's actually in the tree.
    // Sometimes we get events fired on nodes with an ancestor that's
    // marked invisible, for example. In those cases we should still
    // call SerializeChanges (because the change may have affected the
    // ancestor) but we shouldn't fire the event on the node not in the tree.
    if (tree_serializer_->IsInClientTree(aura_obj)) {
      ui::AXEvent event;
      event.id = aura_obj->GetUniqueId();
      event.event_type = event_type;
      if (event_copy.currently_performing_action != ax::mojom::Action::kNone) {
        event.event_from = ax::mojom::EventFrom::kAction;
        event.event_from_action = event_copy.currently_performing_action;
      } else if (event_copy.from_user) {
        event.event_from = ax::mojom::EventFrom::kUser;
      }
      event.action_request_id = event_copy.action_request_id;
      events.push_back(std::move(event));
    }
  }

  // Make sure the focused node is serialized.
  views::AXAuraObjWrapper* focus = cache_->GetFocus();
  if (focus) {
    ui::AXTreeUpdate focused_node_update;
    tree_serializer_->SerializeChanges(focus, &focused_node_update);
    tree_updates.push_back(std::move(focused_node_update));
  }

  if (automation_event_router_interface_) {
    automation_event_router_interface_->DispatchAccessibilityEvents(
        ax_tree_id(), std::move(tree_updates),
        aura::Env::GetInstance()->last_mouse_location(), std::move(events));
  }
}

void AutomationManagerAura::PerformHitTest(
    const ui::AXActionData& original_action) {
  ui::AXActionData action = original_action;
  // Get the display nearest the point.
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(action.target_point);

  // Require a window in |display|; prefer it also be focused.
  aura::Window* root_window = nullptr;
  for (aura::WindowTreeHost* host :
       aura::Env::GetInstance()->window_tree_hosts()) {
    if (display.id() == host->GetDisplayId()) {
      root_window = host->window();
      if (aura::client::GetFocusClient(root_window)->GetFocusedWindow())
        break;
    }
  }

  if (!root_window)
    return;

  // Convert to the root window's coordinates.
  gfx::Point point_in_window(action.target_point);
  ::wm::ConvertPointFromScreen(root_window, &point_in_window);

  // Determine which aura Window is associated with the target point.
  aura::Window* window = root_window->GetEventHandlerForPoint(point_in_window);
  if (!window)
    return;

  // Convert point to local coordinates of the hit window within the root
  // window.
  aura::Window::ConvertPointToTarget(root_window, window, &point_in_window);
  action.target_point = point_in_window;

  // Check for a AX node tree in a remote process (e.g. renderer, mojo app).
  ui::AXTreeID child_ax_tree_id;
  std::string* child_ax_tree_id_ptr = window->GetProperty(ui::kChildAXTreeID);
  if (child_ax_tree_id_ptr)
    child_ax_tree_id = ui::AXTreeID::FromString(*child_ax_tree_id_ptr);

  // If the window has a child AX tree ID, forward the action to the
  // associated AXActionHandlerBase.
  if (child_ax_tree_id != ui::AXTreeIDUnknown()) {
    ui::AXActionHandlerRegistry* registry =
        ui::AXActionHandlerRegistry::GetInstance();
    ui::AXActionHandlerBase* action_handler =
        registry->GetActionHandler(child_ax_tree_id);
    CHECK(action_handler);

    // Convert to pixels for the RenderFrameHost HitTest, if required.
    if (action_handler->RequiresPerformActionPointInPixels()) {
      // The point is in DIPs, so multiply by the device scale factor to
      // get pixels. Don't apply magnification as the action_handler doesn't
      // know about magnification scale (that's applied later in the stack).
      // Specifically, we cannot use WindowTreeHost::ConvertDIPToPixels as that
      // will re-apply the magnification transform. The local point has
      // already been un-transformed when it was converted to local coordinates.
      float device_scale_factor = window->GetHost()->device_scale_factor();
      action.target_point.set_x(action.target_point.x() * device_scale_factor);
      action.target_point.set_y(action.target_point.y() * device_scale_factor);
    }

    action_handler->PerformAction(action);
    return;
  }

  // Fire an event directly on either a view or window.
  views::AXAuraObjWrapper* obj_to_send_event = nullptr;

  // If the window doesn't have a child tree ID, try to fire the event
  // on a View.
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  if (widget) {
    views::View* root_view = widget->GetRootView();
    views::View* hit_view =
        root_view->GetEventHandlerForPoint(action.target_point);
    if (hit_view)
      obj_to_send_event = cache_->GetOrCreate(hit_view);
  }

  // Otherwise, fire the event directly on the Window.
  if (!obj_to_send_event)
    obj_to_send_event = cache_->GetOrCreate(window);
  if (obj_to_send_event) {
    PostEvent(obj_to_send_event->GetUniqueId(), action.hit_test_event_to_fire,
              action.request_id);
  }
}

void AutomationManagerAura::OnSerializeFailure(ax::mojom::Event event_type,
                                               const ui::AXTreeUpdate& update) {
  std::string error_string;
  ui::AXTreeSourceChecker<views::AXAuraObjWrapper*> checker(tree_.get());
  checker.CheckAndGetErrorString(&error_string);

  // Add a crash key so we can figure out why this is happening.
  static crash_reporter::CrashKeyString<256> ax_tree_source_error(
      "ax_tree_source_error");
  ax_tree_source_error.Set(error_string);

  LOG(ERROR) << "Unable to serialize accessibility event!\n"
             << "Event type: " << event_type << "\n"
             << "Error: " << error_string;
}
