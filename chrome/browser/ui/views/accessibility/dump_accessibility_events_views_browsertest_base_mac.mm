// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder_mac.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/views/accessibility/tree/widget_ax_manager.h"

namespace views {

namespace {

// A minimal AXPlatformTreeManager implementation that exists solely to fire
// the sentinel event for Mac accessibility event tests when ViewsAX is
// disabled. This is needed because AXEventRecorderMac::WaitForDoneRecording()
// calls manager_->FireSentinelEventForTesting() to signal the end of the test.
//
// When ViewsAX is enabled, we use the real BrowserAccessibilityManager from
// WidgetAXManager instead.
class ViewsAXPlatformTreeManagerMacForTesting
    : public ui::AXPlatformTreeManager {
 public:
  explicit ViewsAXPlatformTreeManagerMacForTesting(
      gfx::NativeViewAccessible root_element)
      : ui::AXPlatformTreeManager(std::make_unique<ui::AXTree>()),
        root_element_(root_element) {}

  ~ViewsAXPlatformTreeManagerMacForTesting() override = default;

  // AXPlatformTreeManager:
  ui::AXPlatformNode* GetPlatformNodeFromTree(
      ui::AXNodeID node_id) const override {
    return nullptr;
  }
  ui::AXPlatformNode* GetPlatformNodeFromTree(
      const ui::AXNode& node) const override {
    return nullptr;
  }
  ui::AXPlatformNodeDelegate* RootDelegate() const override { return nullptr; }

  void FireSentinelEventForTesting() override {
    // The application deactivated event is used as an end-of-test signal
    // because it never occurs in tests. This mirrors
    // BrowserAccessibilityManagerMac::FireSentinelEventForTesting().
    if (root_element_) {
      NSAccessibilityPostNotification(
          root_element_.Get(),
          NSAccessibilityApplicationDeactivatedNotification);
    }
  }

 private:
  gfx::NativeViewAccessible root_element_;
};

// Static instance to keep the stub manager alive for the duration of the test.
// Only used when ViewsAX is disabled.
base::NoDestructor<std::unique_ptr<ViewsAXPlatformTreeManagerMacForTesting>>
    g_stub_views_manager_for_testing;

}  // namespace

std::unique_ptr<ui::AXEventRecorder> CreateViewsAXEventRecorderMac(
    base::ProcessId pid,
    const ui::AXTreeSelector& selector,
    gfx::NativeViewAccessible root_element,
    WidgetAXManager* widget_ax_manager) {
  base::WeakPtr<ui::AXPlatformTreeManager> manager;

  if (widget_ax_manager && widget_ax_manager->is_enabled()) {
    // ViewsAX is enabled - use the real BrowserAccessibilityManager from
    // WidgetAXManager which properly implements FireSentinelEventForTesting().
    manager = widget_ax_manager->GetAXTreeManagerWeakPtrForTesting();
  } else {
    // ViewsAX is disabled - use our stub manager that can fire the sentinel
    // event.
    *g_stub_views_manager_for_testing =
        std::make_unique<ViewsAXPlatformTreeManagerMacForTesting>(root_element);
    manager = (*g_stub_views_manager_for_testing)->GetWeakPtr();
  }

  return std::make_unique<ui::AXEventRecorderMac>(manager, pid, selector);
}

void CleanupViewsAXEventRecorderMac() {
  g_stub_views_manager_for_testing->reset();
}

}  // namespace views
