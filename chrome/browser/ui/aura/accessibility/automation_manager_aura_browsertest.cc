// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
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
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_tree_source_views.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
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
  auto* registry = ui::AXActionHandlerRegistry::GetInstance();
  return registry->GetAXTreeID(ui::AXActionHandlerRegistry::FrameID(
      rfh->GetProcess()->GetID(), rfh->GetRoutingID()));
}

// A class that installs itself as the sink to handle automation event bundles
// from AutomationManagerAura, then waits until an automation event indicates
// that a given node ID is focused.
class AutomationEventWaiter
    : public extensions::AutomationEventRouterInterface {
 public:
  AutomationEventWaiter() : run_loop_(std::make_unique<base::RunLoop>()) {
    AutomationManagerAura::GetInstance()->set_automation_event_router_interface(
        this);
  }

  virtual ~AutomationEventWaiter() {
    // Don't bother to reconnect to AutomationEventRouter because it's not
    // relevant to the tests.
    AutomationManagerAura::GetInstance()->set_automation_event_router_interface(
        nullptr);
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

  std::unique_ptr<ui::AXEvent> WaitForEvent(ax::mojom::Event event_type) {
    event_type_to_wait_for_ = event_type;
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
    return std::move(most_recent_event_);
  }

  bool WasNodeIdFocused(int node_id) {
    for (size_t i = 0; i < focused_node_ids_.size(); i++)
      if (node_id == focused_node_ids_[i])
        return true;
    return false;
  }

 private:
  // extensions::AutomationEventRouterInterface:
  void DispatchAccessibilityEvents(const ui::AXTreeID& tree_id,
                                   std::vector<ui::AXTreeUpdate> updates,
                                   const gfx::Point& mouse_location,
                                   std::vector<ui::AXEvent> events) override {
    if (node_id_to_wait_for_ != -1) {
      for (const ui::AXTreeUpdate& update : updates) {
        int focused_node_id = update.tree_data.focus_id;
        focused_node_ids_.push_back(focused_node_id);
        if (focused_node_id == node_id_to_wait_for_) {
          node_id_to_wait_for_ = -1;
          run_loop_->Quit();
        }
      }
    }

    if (event_type_to_wait_for_ == ax::mojom::Event::kNone)
      return;

    for (const ui::AXEvent& event : events) {
      if (event.event_type == event_type_to_wait_for_) {
        most_recent_event_ = std::make_unique<ui::AXEvent>(event);
        event_type_to_wait_for_ = ax::mojom::Event::kNone;
        run_loop_->Quit();
      }
    }
  }
  void DispatchAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params) override {}
  void DispatchTreeDestroyedEvent(
      ui::AXTreeID tree_id,
      content::BrowserContext* browser_context) override {}
  void DispatchActionResult(
      const ui::AXActionData& data,
      bool result,
      content::BrowserContext* browser_context = nullptr) override {}
  void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const base::Optional<gfx::Rect>& rect) override {}

  std::unique_ptr<base::RunLoop> run_loop_;
  int node_id_to_wait_for_ = -1;
  std::vector<int> focused_node_ids_;

  std::unique_ptr<ui::AXEvent> most_recent_event_;
  ax::mojom::Event event_type_to_wait_for_ = ax::mojom::Event::kNone;

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
  auto* tree = manager->tree_.get();

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

IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest, ScrollView) {
  auto cache = std::make_unique<views::AXAuraObjCache>();
  auto* cache_ptr = cache.get();
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  manager->set_ax_aura_obj_cache_for_testing(std::move(cache));
  manager->Enable();
  auto* tree = manager->tree_.get();

  // Create a widget with size 200, 200.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = {0, 0, 200, 200};
  widget->Init(std::move(params));

  // Add a ScrollView, with contents consisting of a View of size 1000x2000.
  views::View* root_view = widget->GetRootView();
  auto orig_scroll_view = std::make_unique<views::ScrollView>();
  views::View* scrollable =
      orig_scroll_view->SetContents(std::make_unique<views::View>());
  scrollable->SetBounds(0, 0, 1000, 2000);
  root_view->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  auto full_flex =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1);
  orig_scroll_view->SetProperty(views::kFlexBehaviorKey, full_flex);
  views::View* scroll_view =
      root_view->AddChildView(std::move(orig_scroll_view));
  widget->Show();
  widget->Activate();
  root_view->GetLayoutManager()->Layout(root_view);

  // Get the accessibility data from the scroll view's AXAuraObjCache wrapper.
  views::AXAuraObjWrapper* scroll_view_wrapper =
      cache_ptr->GetOrCreate(scroll_view);
  ui::AXNodeData node_data;
  tree->SerializeNode(scroll_view_wrapper, &node_data);

  // Allow the scroll offsets to be off by 20 pixels due to platform-specific
  // differences.
  constexpr int kAllowedError = 20;

  // The scroll position should be at the top left and the
  // max values should reflect the overall canvas size of (1000, 2000)
  // with a window size of (200, 200).
  EXPECT_EQ(0, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollX));
  EXPECT_EQ(0, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollXMin));
  EXPECT_NEAR(800,
              node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollXMax),
              kAllowedError);
  EXPECT_EQ(0, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollY));
  EXPECT_EQ(0, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollYMin));
  EXPECT_NEAR(1800,
              node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollYMax),
              kAllowedError);

  // Scroll right and check the X position.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kScrollRight;
  scroll_view_wrapper->HandleAccessibleAction(action_data);
  tree->SerializeNode(scroll_view_wrapper, &node_data);
  EXPECT_NEAR(200, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollX),
              kAllowedError);

  // Scroll down and check the Y position.
  action_data.action = ax::mojom::Action::kScrollDown;
  scroll_view_wrapper->HandleAccessibleAction(action_data);
  tree->SerializeNode(scroll_view_wrapper, &node_data);
  EXPECT_NEAR(200, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollY),
              kAllowedError);

  // Scroll to a specific location.
  action_data.action = ax::mojom::Action::kSetScrollOffset;
  action_data.target_point.SetPoint(50, 315);
  scroll_view_wrapper->HandleAccessibleAction(action_data);
  tree->SerializeNode(scroll_view_wrapper, &node_data);
  EXPECT_EQ(50, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollX));
  EXPECT_EQ(315, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollY));
}

IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest, EventFromAction) {
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
  views::View* view2 = new views::View();
  view2->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  view2->GetViewAccessibility().OverrideName("view2");
  widget->GetRootView()->AddChildView(view2);
  views::AXAuraObjWrapper* wrapper2 = cache_ptr->GetOrCreate(view2);

  AutomationEventWaiter waiter;

  // Focus view1, simulating the non-accessibility action, block until we get an
  // accessibility event that shows this view is focused.
  view1->RequestFocus();
  auto event_from_views = waiter.WaitForEvent(ax::mojom::Event::kFocus);
  ASSERT_NE(nullptr, event_from_views.get());
  EXPECT_EQ(ax::mojom::EventFrom::kNone, event_from_views->event_from);

  // Focus view2, simulating the accessibility action, block until we get an
  // accessibility event that shows this view is focused.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  action_data.target_tree_id = manager->tree_.get()->tree_id();
  action_data.target_node_id = wrapper2->GetUniqueId();

  manager->PerformAction(action_data);
  auto event_from_action = waiter.WaitForEvent(ax::mojom::Event::kFocus);
  ASSERT_NE(nullptr, event_from_action.get());
  EXPECT_EQ(ax::mojom::EventFrom::kAction, event_from_action->event_from);

  cache_ptr->set_focused_widget_for_testing(nullptr);

  AddFailureOnWidgetAccessibilityError(widget);
}

// Verify that re-enabling AutomationManagerAura after disable will not cause
// crash.  See https://crbug.com/1177042.
IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest,
                       ReenableDoesNotCauseCrash) {
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  manager->Enable();

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = {0, 0, 200, 200};
  widget->Init(std::move(params));
  widget->Show();
  widget->Activate();

  manager->Disable();

  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, run_loop.QuitWhenIdleClosure());
  run_loop.Run();

  manager->Enable();
}

IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest,
                       AXActionHandlerRegistryUpdates) {
  ui::AXActionHandlerRegistry* registry =
      ui::AXActionHandlerRegistry::GetInstance();
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  ui::AXTreeID tree_id = manager->ax_tree_id();

  // TODO: after Lacros, this should be EQ.
  EXPECT_NE(nullptr, registry->GetActionHandler(tree_id));
  manager->Enable();
  EXPECT_NE(nullptr, registry->GetActionHandler(tree_id));
  manager->Disable();

  // TODO: after Lacros, this should be EQ.
  EXPECT_NE(nullptr, registry->GetActionHandler(tree_id));
  manager->Enable();
  EXPECT_NE(nullptr, registry->GetActionHandler(tree_id));
}
