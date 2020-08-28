// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_action_handler_base.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_bundle_sink.h"
#include "ui/accessibility/ax_tree_id_registry.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/accessibility/accessibility_alert_window.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_root_obj_wrapper.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#endif

// static
AutomationManagerAura* AutomationManagerAura::GetInstance() {
  static base::NoDestructor<AutomationManagerAura> instance;
  return instance.get();
}

void AutomationManagerAura::Enable() {
  enabled_ = true;
  Reset(false);

#if defined(OS_CHROMEOS)
  // Seed the views::AXAuraObjCache with per-display root windows so
  // GetTopLevelWindows() returns the correct values when automation is enabled
  // with multiple displays connected.
  for (aura::Window* root : ash::Shell::GetAllRootWindows())
    cache_->OnRootWindowObjCreated(root);
#endif

  // Send this event immediately to push the initial desktop tree state.
  pending_events_.push_back({current_tree_->GetRoot()->GetUniqueId(),
                             ax::mojom::Event::kLoadComplete, -1,
                             is_performing_action_});
  SendPendingEvents();
  // Intentionally not reset at shutdown since we cannot rely on the shutdown
  // ordering of two base::Singletons.
  cache_->SetDelegate(this);

#if defined(OS_CHROMEOS)
  aura::Window* active_window = ash::window_util::GetActiveWindow();
  if (active_window) {
    views::AXAuraObjWrapper* focus = cache_->GetOrCreate(active_window);
    if (focus)
      PostEvent(focus->GetUniqueId(), ax::mojom::Event::kChildrenChanged);
  }
#endif
}

void AutomationManagerAura::Disable() {
  enabled_ = false;
  Reset(true);
}

void AutomationManagerAura::OnViewEvent(views::View* view,
                                        ax::mojom::Event event_type) {
  CHECK(view);

  if (!enabled_)
    return;

  views::AXAuraObjWrapper* obj = cache_->GetOrCreate(view);
  if (!obj)
    return;

  PostEvent(obj->GetUniqueId(), event_type);
}

void AutomationManagerAura::HandleEvent(ax::mojom::Event event_type) {
  views::AXAuraObjWrapper* obj = current_tree_->GetRoot();
  if (!obj)
    return;

  PostEvent(obj->GetUniqueId(), event_type);
}

void AutomationManagerAura::HandleAlert(const std::string& text) {
  if (alert_window_.get())
    alert_window_->HandleAlert(text);
}

void AutomationManagerAura::PerformAction(const ui::AXActionData& data) {
  CHECK(enabled_);

  base::AutoReset<bool> reset_is_performing_action(&is_performing_action_,
                                                   true);

  // Exclude the do default action, which can trigger too many important events
  // that should not be ignored by clients like focus.
  if (data.action == ax::mojom::Action::kDoDefault)
    is_performing_action_ = false;

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

  PostEvent(parent->GetUniqueId(), ax::mojom::Event::kChildrenChanged);
}

void AutomationManagerAura::OnEvent(views::AXAuraObjWrapper* aura_obj,
                                    ax::mojom::Event event_type) {
  PostEvent(aura_obj->GetUniqueId(), event_type);
}

AutomationManagerAura::AutomationManagerAura()
    : cache_(std::make_unique<views::AXAuraObjCache>()) {
  views::AXEventManager::Get()->AddObserver(this);
}

// Never runs because object is leaked.
AutomationManagerAura::~AutomationManagerAura() = default;

void AutomationManagerAura::Reset(bool reset_serializer) {
  if (!current_tree_) {
    auto desktop_root = std::make_unique<AXRootObjWrapper>(this, cache_.get());
    current_tree_ = std::make_unique<views::AXTreeSourceViews>(
        desktop_root.get(), ax_tree_id(), cache_.get());
    cache_->CreateOrReplace(std::move(desktop_root));
  }
  if (reset_serializer) {
    current_tree_serializer_.reset();
    alert_window_.reset();
  } else {
    current_tree_serializer_ =
        std::make_unique<AuraAXTreeSerializer>(current_tree_.get());
#if defined(OS_CHROMEOS)
    ash::Shell* shell = ash::Shell::Get();
    // Windows within the overlay container get moved to the new monitor when
    // the primary display gets swapped.
    alert_window_ = std::make_unique<views::AccessibilityAlertWindow>(
        shell->GetContainer(shell->GetPrimaryRootWindow(),
                            ash::kShellWindowId_OverlayContainer),
        cache_.get());
#endif  // defined(OS_CHROMEOS)
  }
}

void AutomationManagerAura::PostEvent(int id,
                                      ax::mojom::Event event_type,
                                      int action_request_id) {
  pending_events_.push_back(
      {id, event_type, action_request_id, is_performing_action_});

  if (processing_posted_)
    return;

  processing_posted_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AutomationManagerAura::SendPendingEvents,
                                base::Unretained(this)));
}

void AutomationManagerAura::SendPendingEvents() {
  processing_posted_ = false;
  if (!enabled_)
    return;

  if (!current_tree_serializer_)
    return;

  std::vector<ui::AXTreeUpdate> tree_updates;
  std::vector<ui::AXEvent> events;
  auto pending_events_copy = pending_events_;
  pending_events_.clear();
  for (auto& event_copy : pending_events_copy) {
    int id = event_copy.id;
    ax::mojom::Event event_type = event_copy.event_type;
    auto* aura_obj = cache_->Get(id);
    if (!aura_obj)
      continue;

    ui::AXTreeUpdate update;
    if (!current_tree_serializer_->SerializeChanges(aura_obj, &update)) {
      OnSerializeFailure(event_type, update);
      return;
    }
    tree_updates.push_back(update);

    // Fire the event on the node, but only if it's actually in the tree.
    // Sometimes we get events fired on nodes with an ancestor that's
    // marked invisible, for example. In those cases we should still
    // call SerializeChanges (because the change may have affected the
    // ancestor) but we shouldn't fire the event on the node not in the tree.
    if (current_tree_serializer_->IsInClientTree(aura_obj)) {
      ui::AXEvent event;
      event.id = aura_obj->GetUniqueId();
      event.event_type = event_type;
      if (event_copy.is_performing_action)
        event.event_from = ax::mojom::EventFrom::kAction;
      event.action_request_id = event_copy.action_request_id;
      events.push_back(event);
    }
  }

  // Make sure the focused node is serialized.
  views::AXAuraObjWrapper* focus = cache_->GetFocus();
  if (focus) {
    ui::AXTreeUpdate focused_node_update;
    current_tree_serializer_->SerializeChanges(focus, &focused_node_update);
    tree_updates.push_back(focused_node_update);
  }

  if (event_bundle_sink_) {
    event_bundle_sink_->DispatchAccessibilityEvents(
        ax_tree_id(), std::move(tree_updates),
        aura::Env::GetInstance()->last_mouse_location(), std::move(events));
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
  // associated AXActionHandlerBase.
  if (child_ax_tree_id != ui::AXTreeIDUnknown()) {
    ui::AXTreeIDRegistry* registry = ui::AXTreeIDRegistry::GetInstance();
    ui::AXActionHandlerBase* action_handler =
        registry->GetActionHandler(child_ax_tree_id);
    CHECK(action_handler);

    // Convert to pixels for the RenderFrameHost HitTest, if required.
    if (action_handler->RequiresPerformActionPointInPixels())
      window->GetHost()->ConvertDIPToPixels(&action.target_point);

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
    if (hit_view) {
      obj_to_send_event = cache_->GetOrCreate(hit_view);
    }
  }

  // Otherwise, fire the event directly on the Window.
  if (!obj_to_send_event)
    obj_to_send_event = cache_->GetOrCreate(window);
  if (obj_to_send_event) {
    PostEvent(obj_to_send_event->GetUniqueId(), action.hit_test_event_to_fire,
              action.request_id);
  }
#endif
}

void AutomationManagerAura::OnSerializeFailure(ax::mojom::Event event_type,
                                               const ui::AXTreeUpdate& update) {
  std::string error_string;
  ui::AXTreeSourceChecker<views::AXAuraObjWrapper*, ui::AXNodeData,
                          ui::AXTreeData>
      checker(current_tree_.get());
  checker.CheckAndGetErrorString(&error_string);

  // Add a crash key so we can figure out why this is happening.
  static crash_reporter::CrashKeyString<256> ax_tree_source_error(
      "ax_tree_source_error");
  ax_tree_source_error.Set(error_string);

  LOG(ERROR) << "Unable to serialize accessibility event!\n"
             << "Event type: " << event_type << "\n"
             << "Error: " << error_string << "\n"
             << "Update: " << update.ToString();
}
