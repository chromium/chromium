// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/inspect/ax_event_recorder_auralinux.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/views/accessibility/tree/widget_ax_manager.h"

namespace views {

namespace {

// A minimal AXPlatformTreeManager implementation that exists solely to satisfy
// the non-null manager requirement of AXEventRecorderAuraLinux for Views-layer
// tests when ViewsAX is disabled. On Linux, ProcessATKEvent() early-returns
// when the manager is null or its RootDelegate() returns null (a signal that
// the tree is being destroyed).
//
// This stub provides a non-null RootDelegate() via a no-op
// AXPlatformNodeDelegate so that events are properly recorded.
// Unlike the Mac equivalent, Linux does not need FireSentinelEventForTesting().
//
// When ViewsAX is enabled, we use the real BrowserAccessibilityManager from
// WidgetAXManager instead.
class ViewsAXPlatformTreeManagerLinuxForTesting
    : public ui::AXPlatformTreeManager {
 public:
  ViewsAXPlatformTreeManagerLinuxForTesting()
      : ui::AXPlatformTreeManager(std::make_unique<ui::AXTree>()) {}

  ~ViewsAXPlatformTreeManagerLinuxForTesting() override = default;

  // AXPlatformTreeManager:
  ui::AXPlatformNode* GetPlatformNodeFromTree(
      ui::AXNodeID node_id) const override {
    return nullptr;
  }
  ui::AXPlatformNode* GetPlatformNodeFromTree(
      const ui::AXNode& node) const override {
    return nullptr;
  }
  ui::AXPlatformNodeDelegate* RootDelegate() const override {
    // Return a no-op delegate so that ProcessATKEvent() does not early-return.
    // AXPlatformNodeDelegate has no pure virtual methods and safe defaults.
    static base::NoDestructor<ui::AXPlatformNodeDelegate> stub_delegate;
    return &*stub_delegate;
  }

 private:
};

// Static instance to keep the stub manager alive for the duration of the test.
// Only used when ViewsAX is disabled.
base::NoDestructor<std::unique_ptr<ViewsAXPlatformTreeManagerLinuxForTesting>>
    g_stub_views_manager_for_testing;

}  // namespace

std::unique_ptr<ui::AXEventRecorder> CreateViewsAXEventRecorderAuraLinux(
    base::ProcessId pid,
    const ui::AXTreeSelector& selector,
    WidgetAXManager* widget_ax_manager) {
  base::WeakPtr<ui::AXPlatformTreeManager> manager;

  if (widget_ax_manager && widget_ax_manager->is_enabled()) {
    // ViewsAX is enabled - use the real BrowserAccessibilityManager from
    // WidgetAXManager.
    manager = widget_ax_manager->GetAXTreeManagerWeakPtrForTesting();
  } else {
    // ViewsAX is disabled - use our stub manager that provides a non-null
    // RootDelegate().
    *g_stub_views_manager_for_testing =
        std::make_unique<ViewsAXPlatformTreeManagerLinuxForTesting>();
    manager = (*g_stub_views_manager_for_testing)->GetWeakPtr();
  }

  return std::make_unique<ui::AXEventRecorderAuraLinux>(manager, pid, selector);
}

void CleanupViewsAXEventRecorderAuraLinux() {
  g_stub_views_manager_for_testing->reset();
}

}  // namespace views
