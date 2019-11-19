// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/views/accessibility_checker.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_event_bundle_sink.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_tree_source_views.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Given an AXTreeSourceViews and a node within that tree, recursively search
// for all nodes who have a child tree id of |target_ax_tree_id|, meaning
// that they're a parent of a particular web contents.
void FindAllHostsOfWebContentsWithAXTreeID(
    views::AXTreeSourceViews* tree,
    views::AXAuraObjWrapper* node,
    ui::AXTreeID target_ax_tree_id,
    std::vector<views::AXAuraObjWrapper*>* web_hosts) {
  ui::AXNodeData node_data;
  tree->SerializeNode(node, &node_data);
  if (ui::AXTreeID::FromString(node_data.GetStringAttribute(
          ax::mojom::StringAttribute::kChildTreeId)) == target_ax_tree_id) {
    web_hosts->push_back(node);
  }

  std::vector<views::AXAuraObjWrapper*> children;
  tree->GetChildren(node, &children);
  for (auto* child : children) {
    FindAllHostsOfWebContentsWithAXTreeID(tree, child, target_ax_tree_id,
                                          web_hosts);
  }
}

// A helper to retrieve an ax tree id given a RenderFrameHost.
ui::AXTreeID GetAXTreeIDFromRenderFrameHost(content::RenderFrameHost* rfh) {
  auto* registry = ui::AXTreeIDRegistry::GetInstance();
  return registry->GetAXTreeID(ui::AXTreeIDRegistry::FrameID(
      rfh->GetProcess()->GetID(), rfh->GetRoutingID()));
}

// A class that installs itself as the sink to handle automation event bundles
// from AutomationManagerAura, then waits until an automation event indicates
// that a given node ID is focused.
class AutomationEventWaiter : public ui::AXEventBundleSink {
 public:
  AutomationEventWaiter() : run_loop_(std::make_unique<base::RunLoop>()) {
    AutomationManagerAura::GetInstance()->set_event_bundle_sink(this);
  }

  ~AutomationEventWaiter() override {
    // Don't bother to reconnect to AutomationEventRouter because it's not
    // relevant to the tests.
    AutomationManagerAura::GetInstance()->set_event_bundle_sink(nullptr);
  }

  // Returns immediately if the node with AXAuraObjCache ID |node_id|
  // has ever been focused, otherwise spins a loop until that node is
  // focused.
  void WaitForNodeIdToBeFocused(int node_id) {
    if (WasNodeIdFocused(node_id))
      return;

    node_id_to_wait_for_ = node_id;
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  bool WasNodeIdFocused(int node_id) {
    for (size_t i = 0; i < focused_node_ids_.size(); i++)
      if (node_id == focused_node_ids_[i])
        return true;
    return false;
  }

 private:
  // ui::AXEventBundleSink:
  void DispatchAccessibilityEvents(const ui::AXTreeID& tree_id,
                                   std::vector<ui::AXTreeUpdate> updates,
                                   const gfx::Point& mouse_location,
                                   std::vector<ui::AXEvent> events) override {
    for (const ui::AXTreeUpdate& update : updates) {
      int focused_node_id = update.tree_data.focus_id;
      focused_node_ids_.push_back(focused_node_id);
      if (focused_node_id == node_id_to_wait_for_)
        run_loop_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  int node_id_to_wait_for_ = 0;
  std::vector<int> focused_node_ids_;

  DISALLOW_COPY_AND_ASSIGN(AutomationEventWaiter);
};

}  // namespace

typedef InProcessBrowserTest AutomationManagerAuraBrowserTest;

// A WebContents can be "hooked up" to the Chrome OS Desktop accessibility
// tree two different ways: via its aura::Window, and via a views::WebView.
// This test makes sure that we don't hook up both simultaneously, leading
// to the same web page appearing in the overall tree twice.
IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest, WebAppearsOnce) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  manager->Enable();
  auto* tree = manager->current_tree_.get();

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("data:text/html;charset=utf-8,<button autofocus>Click me</button>"));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  WaitForAccessibilityTreeToContainNodeWithName(web_contents, "Click me");

  auto* frame_host = web_contents->GetMainFrame();
  ui::AXTreeID ax_tree_id = GetAXTreeIDFromRenderFrameHost(frame_host);
  ASSERT_NE(ax_tree_id, ui::AXTreeIDUnknown());

  std::vector<views::AXAuraObjWrapper*> web_hosts;
  FindAllHostsOfWebContentsWithAXTreeID(tree, tree->GetRoot(), ax_tree_id,
                                        &web_hosts);

  EXPECT_EQ(1U, web_hosts.size());
  if (web_hosts.size() == 1) {
    ui::AXNodeData node_data;
    tree->SerializeNode(web_hosts[0], &node_data);
    EXPECT_EQ(ax::mojom::Role::kWebView, node_data.role);
  } else {
    for (size_t i = 0; i < web_hosts.size(); i++) {
      ui::AXNodeData node_data;
      tree->SerializeNode(web_hosts[i], &node_data);
      LOG(ERROR) << i << ": " << node_data.ToString();
    }
  }
}

IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest,
                       TransientFocusChangesAreSuppressed) {
  auto cache = std::make_unique<views::AXAuraObjCache>();
  auto* cache_ptr = cache.get();
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  manager->set_ax_aura_obj_cache_for_testing(std::move(cache));
  manager->Enable();

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = {0, 0, 200, 200};
  widget->Init(std::move(params));
  widget->Show();
  widget->Activate();

  cache_ptr->set_focused_widget_for_testing(widget);

  views::View* view1 = new views::View();
  view1->GetViewAccessibility().OverrideName("view1");
  view1->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  widget->GetRootView()->AddChildView(view1);
  views::AXAuraObjWrapper* wrapper1 = cache_ptr->GetOrCreate(view1);
  views::View* view2 = new views::View();
  view2->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  view2->GetViewAccessibility().OverrideName("view2");
  widget->GetRootView()->AddChildView(view2);
  views::AXAuraObjWrapper* wrapper2 = cache_ptr->GetOrCreate(view2);
  views::View* view3 = new views::View();
  view3->GetViewAccessibility().OverrideName("view3");
  view3->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  widget->GetRootView()->AddChildView(view3);
  views::AXAuraObjWrapper* wrapper3 = cache_ptr->GetOrCreate(view3);

  AutomationEventWaiter waiter;

  // Focus view1, then block until we get an accessibility event that
  // shows this view is focused.
  view1->RequestFocus();
  waiter.WaitForNodeIdToBeFocused(wrapper1->GetUniqueId());

  EXPECT_TRUE(waiter.WasNodeIdFocused(wrapper1->GetUniqueId()));
  EXPECT_FALSE(waiter.WasNodeIdFocused(wrapper2->GetUniqueId()));
  EXPECT_FALSE(waiter.WasNodeIdFocused(wrapper3->GetUniqueId()));

  // Now focus view2 and then view3. We shouldn't ever get an event
  // showing view2 as focused, just view3.
  view2->RequestFocus();
  view3->RequestFocus();
  waiter.WaitForNodeIdToBeFocused(wrapper3->GetUniqueId());

  EXPECT_TRUE(waiter.WasNodeIdFocused(wrapper1->GetUniqueId()));
  EXPECT_FALSE(waiter.WasNodeIdFocused(wrapper2->GetUniqueId()));
  EXPECT_TRUE(waiter.WasNodeIdFocused(wrapper3->GetUniqueId()));

  cache_ptr->set_focused_widget_for_testing(nullptr);

  AddFailureOnWidgetAccessibilityError(widget);
}
