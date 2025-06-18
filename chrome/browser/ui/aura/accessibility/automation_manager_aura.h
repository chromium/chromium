// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_
#define CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/views/accessibility/tree/views_ax_manager.h"

namespace extensions {
class AutomationEventRouterInterface;
}  // namespace extensions

// Manages a tree of automation nodes backed by aura constructs.
class AutomationManagerAura : public views::ViewsAXManager,
                              public extensions::AutomationEventRouterObserver {
 public:
  AutomationManagerAura(const AutomationManagerAura&) = delete;
  AutomationManagerAura& operator=(const AutomationManagerAura&) = delete;

  // Get the single instance of this class.
  static AutomationManagerAura* GetInstance();

  // views::ViewsAXManager:
  void Enable() override;
  void Disable() override;

  // Handle an event fired upon the root view.
  // TODO(https://crbug.com/40672441): Investigate whether this can be
  // refactored away.
  void HandleEvent(ax::mojom::Event event_type, bool from_user);

  // AutomationEventRouterObserver:
  void AllAutomationExtensionsGone() override;
  void ExtensionListenerAdded() override;

  void set_automation_event_router_interface(
      extensions::AutomationEventRouterInterface* router) {
    automation_event_router_interface_ = router;
  }

 private:
  friend class base::NoDestructor<AutomationManagerAura>;
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest, ScrollView);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest, TableView);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest, WebAppearsOnce);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest, EventFromAction);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest,
                           SerializeOnDataChanged);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest,
                           GetFocusOnChildTree);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest,
                           TransientFocusChangesAreSuppressed);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest,
                           ViewAddedAndRemovedFromParent);
  FRIEND_TEST_ALL_PREFIXES(AutomationManagerAuraBrowserTest,
                           ViewReparentedBetweenViews);

  AutomationManagerAura();
  ~AutomationManagerAura() override;

  void DispatchAccessibilityEvents(const ui::AXTreeID& tree_id,
                                   std::vector<ui::AXTreeUpdate> tree_updates,
                                   const gfx::Point& mouse_location,
                                   std::vector<ui::AXEvent> events) override;

  // The handler for AXEvents (e.g. the extensions subsystem in production, or
  // a fake for tests).
  raw_ptr<extensions::AutomationEventRouterInterface>
      automation_event_router_interface_ = nullptr;

  base::ScopedObservation<extensions::AutomationEventRouter,
                          extensions::AutomationEventRouterObserver>
      automation_event_router_observer_{this};
};

#endif  // CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AUTOMATION_MANAGER_AURA_H_
