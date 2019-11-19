// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/ui/aura/accessibility/automation_manager_aura.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chromecast/browser/accessibility/accessibility_manager.h"
#include "chromecast/browser/cast_browser_process.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_bundle_sink.h"
#include "ui/accessibility/ax_tree_id_registry.h"
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

// static
AutomationManagerAura* AutomationManagerAura::GetInstance() {
  static base::NoDestructor<AutomationManagerAura> instance;
  return instance.get();
}

void AutomationManagerAura::Enable() {
  enabled_ = true;
  Reset(false);

  SendEvent(current_tree_->GetRoot(), ax::mojom::Event::kLoadComplete);
  cache_.SetDelegate(this);

  aura::Window* active_window =
      chromecast::shell::CastBrowserProcess::GetInstance()
          ->accessibility_manager()
          ->window_tree_host()
          ->window();
  if (active_window) {
    views::AXAuraObjWrapper* focus = cache_.GetOrCreate(active_window);
    SendEvent(focus, ax::mojom::Event::kChildrenChanged);
  }

  // Notify the browser process of a change to accessibility state so it
  // can notify any out of out of process (non chrome renderers) that need
  // to know.
  chromecast::shell::CastBrowserProcess::GetInstance()
      ->AccessibilityStateChanged(true);
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

  views::AXAuraObjWrapper* obj = cache_.GetOrCreate(view);
  if (!obj)
    return;

  // Ignore toplevel window activate and deactivate events. These are causing
  // issues with ChromeOS accessibility tests and are currently only used on
  // desktop Linux platforms.
  // TODO(https://crbug.com/89717): Need to harmonize the firing of
  // accessibility events between platforms.
  if (event_type == ax::mojom::Event::kWindowActivated ||
      event_type == ax::mojom::Event::kWindowDeactivated) {
    return;
  }

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
  views::AXAuraObjWrapper* obj = cache_.Get(id);
  if (obj)
    SendEvent(obj, event_type);
}

void AutomationManagerAura::HandleAlert(const std::string& text) {
  if (alert_window_.get())
    alert_window_->HandleAlert(text);
}

void AutomationManagerAura::PerformAction(const ui::AXActionData& data) {
  CHECK(enabled_);

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
    : enabled_(false), processing_events_(false) {
  views::AXEventManager::Get()->AddObserver(this);
}

// Never runs because object is leaked.
AutomationManagerAura::~AutomationManagerAura() = default;

void AutomationManagerAura::Reset(bool reset_serializer) {
  if (!current_tree_) {
    desktop_root_ = std::make_unique<AXRootObjWrapper>(this, &cache_);
    current_tree_ = std::make_unique<AXTreeSourceAura>(desktop_root_.get(),
                                                       ax_tree_id(), &cache_);
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
        views::AXAuraObjCache::GetInstance());
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
    LOG(ERROR) << "Unable to serialize one accessibility event: "
               << update.ToString();
    return;
  }
  tree_updates.push_back(update);

  // Make sure the focused node is serialized.
  views::AXAuraObjWrapper* focus = cache_.GetFocus();
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
