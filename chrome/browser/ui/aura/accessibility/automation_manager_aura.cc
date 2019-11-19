// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_bundle_sink.h"
#include "ui/accessibility/ax_tree_id_registry.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/accessibility/platform/aura_window_properties.h"
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

  SendEvent(current_tree_->GetRoot(), ax::mojom::Event::kLoadComplete);
  // Intentionally not reset at shutdown since we cannot rely on the shutdown
  // ordering of two base::Singletons.
  cache_->SetDelegate(this);

#if defined(OS_CHROMEOS)
  aura::Window* active_window = ash::window_util::GetActiveWindow();
  if (active_window) {
    views::AXAuraObjWrapper* focus = cache_->GetOrCreate(active_window);
    if (focus)
      SendEvent(focus, ax::mojom::Event::kChildrenChanged);
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

  // Post a task to handle the event at the end of the current call stack.
  // This helps us avoid firing accessibility events for transient changes.
  // because there's a chance that the underlying object being wrapped could
  // be deleted, pass the ID of the object rather than the object pointer.
  int32_t id = obj->GetUniqueId();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AutomationManagerAura::SendEventOnObjectById,
                                base::Unretained(this), id, event_type));
}

void AutomationManagerAura::HandleEvent(ax::mojom::Event event_type) {
  views::AXAuraObjWrapper* obj = current_tree_->GetRoot();
  if (!obj)
    return;

  AutomationManagerAura::SendEvent(obj, event_type);
}

void AutomationManagerAura::SendEventOnObjectById(int32_t id,
                                                  ax::mojom::Event event_type) {
  views::AXAuraObjWrapper* obj = cache_->Get(id);
  if (obj)
    SendEvent(obj, event_type);
}

void AutomationManagerAura::HandleAlert(const std::string& text) {
  if (alert_window_.get())
    alert_window_->HandleAlert(text);
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
    : enabled_(false),
      processing_events_(false),
      cache_(std::make_unique<views::AXAuraObjCache>()) {
  views::AXEventManager::Get()->AddObserver(this);
}

// Never runs because object is leaked.
AutomationManagerAura::~AutomationManagerAura() = default;

void AutomationManagerAura::Reset(bool reset_serializer) {
  if (!current_tree_) {
    desktop_root_ = std::make_unique<AXRootObjWrapper>(this, cache_.get());
    current_tree_ = std::make_unique<views::AXTreeSourceViews>(
        desktop_root_.get(), ax_tree_id(), cache_.get());
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

  std::vector<ui::AXTreeUpdate> tree_updates;
  ui::AXTreeUpdate update;
  if (!current_tree_serializer_->SerializeChanges(aura_obj, &update)) {
    OnSerializeFailure(event_type, update);
    return;
  }
  tree_updates.push_back(update);

  // Make sure the focused node is serialized.
  views::AXAuraObjWrapper* focus = cache_->GetFocus();
  if (focus) {
    ui::AXTreeUpdate focused_node_update;
    current_tree_serializer_->SerializeChanges(focus, &focused_node_update);
    tree_updates.push_back(focused_node_update);
  }

  std::vector<ui::AXEvent> events;
  // Fire the event on the node, but only if it's actually in the tree.
  // Sometimes we get events fired on nodes with an ancestor that's
  // marked invisible, for example. In those cases we should still
  // call SerializeChanges (because the change may have affected the
  // ancestor) but we shouldn't fire the event on the node not in the tree.
  if (current_tree_serializer_->IsInClientTree(aura_obj)) {
    ui::AXEvent event;
    event.id = aura_obj->GetUniqueId();
    event.event_type = event_type;
    events.push_back(event);
  }

  if (event_bundle_sink_) {
    event_bundle_sink_->DispatchAccessibilityEvents(
        ax_tree_id(), std::move(tree_updates),
        aura::Env::GetInstance()->last_mouse_location(), std::move(events));
  }

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
  // associated AXActionHandler.
  if (child_ax_tree_id != ui::AXTreeIDUnknown()) {
    ui::AXTreeIDRegistry* registry = ui::AXTreeIDRegistry::GetInstance();
    ui::AXActionHandler* action_handler =
        registry->GetActionHandler(child_ax_tree_id);
    CHECK(action_handler);

    // Convert to pixels for the RenderFrameHost HitTest, if required.
    if (action_handler->RequiresPerformActionPointInPixels())
      window->GetHost()->ConvertDIPToPixels(&action.target_point);

    action_handler->PerformAction(action);
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
  views::AXAuraObjWrapper* window_wrapper = cache_->GetOrCreate(window);
  if (window_wrapper)
    SendEvent(window_wrapper, action.hit_test_event_to_fire);
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

  LOG(FATAL) << "Unable to serialize accessibility event!\n"
             << "Event type: " << event_type << "\n"
             << "Error: " << error_string << "\n"
             << "Update: " << update.ToString();
}
