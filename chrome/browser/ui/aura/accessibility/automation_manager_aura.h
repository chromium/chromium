// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_
#define CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/accessibility/ax_tree_source_views.h"

namespace extensions {
class AutomationEventRouterInterface;
}  // namespace extensions

namespace views {
class AccessibilityAlertWindow;
class AXAuraObjWrapper;
class View;
}  // namespace views

using AuraAXTreeSerializer = ui::AXTreeSerializer<
    views::AXAuraObjWrapper*,
    std::vector<raw_ptr<views::AXAuraObjWrapper, VectorExperimental>>,
    ui::AXTreeUpdate*,
    ui::AXTreeData*,
    ui::AXNodeData>;

// Manages a tree of automation nodes backed by aura constructs.
class AutomationManagerAura : public ui::AXActionHandler,
                              public views::AXAuraObjCache::Delegate,
                              public views::AXEventObserver,
                              public extensions::AutomationEventRouterObserver {
 public:
  AutomationManagerAura(const AutomationManagerAura&) = delete;
  AutomationManagerAura& operator=(const AutomationManagerAura&) = delete;

  // Get the single instance of this class.
  static AutomationManagerAura* GetInstance();

  // Enable automation support for views.
  void Enable();

  // Disable automation support for views.
  void Disable();

  // Handle an event fired upon the root view.
  void HandleEvent(ax::mojom::Event event_type, bool from_user);

  // Handles a textual alert.
  void HandleAlert(const std::string& text);

  // AXActionHandlerBase implementation.
  void PerformAction(const ui::AXActionData& data) override;

  void SetA11yOverrideWindow(aura::Window* a11y_override_window);

  // views::AXAuraObjCache::Delegate implementation.
  void OnChildWindowRemoved(views::AXAuraObjWrapper* parent) override;
  void OnEvent(views::AXAuraObjWrapper* aura_obj,
               ax::mojom::Event event_type) override;

  // views::AXEventObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override;
  void OnVirtualViewEvent(views::AXVirtualView* virtual_view,
                          ax::mojom::Event event_type) override;

  // AutomationEventRouterObserver:
  void AllAutomationExtensionsGone() override;
  void ExtensionListenerAdded() override;

  void set_automation_event_router_interface(
      extensions::AutomationEventRouterInterface* router) {
    automation_event_router_interface_ = router;
  }

  void set_ax_aura_obj_cache_for_testing(
      std::unique_ptr<views::AXAuraObjCache> cache) {
    cache_ = std::move(cache);
  }

 private:
  friend class base::NoDestructor<AutomationManagerAura>;

  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest, ScrollView);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest, TableView);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest, WebAppearsOnce);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest, EventFromAction);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest,
                           GetFocusOnChildTree);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest,
                           TransientFocusChangesAreSuppressed);

  AutomationManagerAura();
  ~AutomationManagerAura() override;

  // Reset state in this manager. If |reset_serializer| is true, reset the
  // serializer to save memory.
  void Reset(bool reset_serializer);

  void PostEvent(int32_t id,
                 ax::mojom::Event event_type,
                 int action_request_id = -1,
                 bool from_user = false);

  void SendPendingEvents();

  void PerformHitTest(const ui::AXActionData& data);

  // Logs an error with details about a serialization failure.
  void OnSerializeFailure(ax::mojom::Event event_type,
                          const ui::AXTreeUpdate& update);

  // Whether automation support for views is enabled.
  bool enabled_ = false;

  std::unique_ptr<views::AXAuraObjCache> cache_;

  // Holds the active views-based accessibility tree. A tree currently consists
  // of all views descendant to a |Widget| (see |AXTreeSourceViews|).
  // A tree becomes active when an event is fired on a descendant view.
  std::unique_ptr<views::AXTreeSourceViews> tree_;

  // Serializes incremental updates on the currently active tree
  // |tree_|.
  std::unique_ptr<AuraAXTreeSerializer> tree_serializer_;

  bool processing_posted_ = false;

  struct Event {
    int id;
    ax::mojom::Event event_type;
    int action_request_id;
    ax::mojom::Action currently_performing_action;
    bool from_user;
  };

  std::vector<Event> pending_events_;

  // The handler for AXEvents (e.g. the extensions subsystem in production, or
  // a fake for tests).
  raw_ptr<extensions::AutomationEventRouterInterface>
      automation_event_router_interface_ = nullptr;

  std::unique_ptr<views::AccessibilityAlertWindow> alert_window_;

  ax::mojom::Action currently_performing_action_ = ax::mojom::Action::kNone;

  base::ScopedObservation<extensions::AutomationEventRouter,
                          extensions::AutomationEventRouterObserver>
      automation_event_router_observer_{this};

  bool send_window_state_on_enable_ = true;
};

#endif  // CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_
