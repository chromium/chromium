// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_
#define CHROMECAST_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "chromecast/browser/ui/aura/accessibility/ax_tree_source_aura.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/accessibility/ax_tree_source_views.h"

class AXRootObjWrapper;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace ui {
class AXEventBundleSink;
}

namespace views {
class AccessibilityAlertWindow;
class AXAuraObjWrapper;
class View;
}  // namespace views

using AuraAXTreeSerializer = ui::
    AXTreeSerializer<views::AXAuraObjWrapper*, ui::AXNodeData, ui::AXTreeData>;

// Manages a tree of automation nodes.
class AutomationManagerAura : public ui::AXActionHandler,
                              public views::AXAuraObjCache::Delegate,
                              public views::AXEventObserver {
 public:
  // Get the single instance of this class.
  static AutomationManagerAura* GetInstance();

  // Enable automation support for views.
  void Enable();

  // Disable automation support for views.
  void Disable();

  // Handle an event fired upon the root view.
  void HandleEvent(ax::mojom::Event event_type);

  void HandleAlert(const std::string& text);

  // AXActionHandler implementation.
  void PerformAction(const ui::AXActionData& data) override;

  // views::AXAuraObjCache::Delegate implementation.
  void OnChildWindowRemoved(views::AXAuraObjWrapper* parent) override;
  void OnEvent(views::AXAuraObjWrapper* aura_obj,
               ax::mojom::Event event_type) override;

  // views::AXEventObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override;

  void set_event_bundle_sink(ui::AXEventBundleSink* sink) {
    event_bundle_sink_ = sink;
  }

  int32_t GetIDFromWindow(aura::Window* window) { return cache_.GetID(window); }

 private:
  friend class base::NoDestructor<AutomationManagerAura>;

  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest, WebAppearsOnce);

  AutomationManagerAura();
  ~AutomationManagerAura() override;

  void SendEventOnObjectById(int32_t id, ax::mojom::Event event_type);

  // Reset state in this manager. If |reset_serializer| is true, reset the
  // serializer to save memory.
  void Reset(bool reset_serializer);

  void SendEvent(views::AXAuraObjWrapper* aura_obj,
                 ax::mojom::Event event_type);

  // Whether automation support for views is enabled.
  bool enabled_;

  // Root object representing the entire desktop. Must outlive |current_tree_|.
  std::unique_ptr<AXRootObjWrapper> desktop_root_;

  // Holds the active views-based accessibility tree. A tree currently consists
  // of all views descendant to a |Widget| (see |AXTreeSourceViews|).
  // A tree becomes active when an event is fired on a descendant view.
  std::unique_ptr<AXTreeSourceAura> current_tree_;

  // Serializes incremental updates on the currently active tree
  // |current_tree_|.
  std::unique_ptr<AuraAXTreeSerializer> current_tree_serializer_;

  bool processing_events_;

  std::vector<std::pair<views::AXAuraObjWrapper*, ax::mojom::Event>>
      pending_events_;

  // The handler for AXEvents (e.g. the extensions subsystem in production, or
  // a fake for tests).
  ui::AXEventBundleSink* event_bundle_sink_ = nullptr;

  std::unique_ptr<views::AccessibilityAlertWindow> alert_window_;

  views::AXAuraObjCache cache_;

  DISALLOW_COPY_AND_ASSIGN(AutomationManagerAura);
};

#endif  // CHROMECAST_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_
