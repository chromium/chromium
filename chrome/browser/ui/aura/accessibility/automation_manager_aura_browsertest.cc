// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/views/accessibility_checker.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_tree_source_views.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/test_table_model.h"
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

  tree->CacheChildrenIfNeeded(node);
  auto num_children = tree->GetChildCount(node);
  for (size_t i = 0; i < num_children; i++) {
    auto* child = tree->ChildAt(node, i);
    FindAllHostsOfWebContentsWithAXTreeID(tree, child, target_ax_tree_id,
                                          web_hosts);
  }
  tree->ClearChildCache(node);
}

// A helper to retrieve an ax tree id given a RenderFrameHost.
ui::AXTreeID GetAXTreeIDFromRenderFrameHost(content::RenderFrameHost* rfh) {
  auto* registry = ui::AXActionHandlerRegistry::GetInstance();
  return registry->GetAXTreeID(ui::AXActionHandlerRegistry::FrameID(
      rfh->GetProcess()->GetID(), rfh->GetRoutingID()));
}

// A class that installs itself as the sink to handle automation event bundles
// from AutomationManagerAura, then waits until an automation event indicates
// that a given node ID is focused or an AX event is sent.
class AutomationEventWaiter
    : public extensions::AutomationEventRouterInterface {
 public:
  AutomationEventWaiter() : run_loop_(std::make_unique<base::RunLoop>()) {
    AutomationManagerAura::GetInstance()->set_automation_event_router_interface(
        this);
  }

  AutomationEventWaiter(const AutomationEventWaiter&) = delete;
  AutomationEventWaiter& operator=(const AutomationEventWaiter&) = delete;

  virtual ~AutomationEventWaiter() {
    // Don't bother to reconnect to AutomationEventRouter because it's not
    // relevant to the tests.
    AutomationManagerAura::GetInstance()->set_automation_event_router_interface(
        nullptr);
  }

  // Returns immediately if the node with AXAuraObjCache ID |node_id|
  // has ever been focused, otherwise spins a loop until that node is
  // focused.
  void WaitForNodeIdToBeFocused(ui::AXNodeID node_id) {
    if (WasNodeIdFocused(node_id))
      return;

    node_id_to_wait_for_ = node_id;
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  std::unique_ptr<ui::AXEvent> WaitForEvent(
      ax::mojom::Event event_type,
      ui::AXNodeID target_node_id = ui::kInvalidAXNodeID) {
    event_type_to_wait_for_ = event_type;
    event_target_node_id_to_wait_for_ = target_node_id;
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
    return std::move(matched_wait_for_event_);
  }

  bool WasNodeIdFocused(int node_id) {
    for (size_t i = 0; i < focused_node_ids_.size(); i++)
      if (node_id == focused_node_ids_[i])
        return true;
    return false;
  }

  ui::AXTree* ax_tree() { return &ax_tree_; }

 private:
  // extensions::AutomationEventRouterInterface:
  void DispatchAccessibilityEvents(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const gfx::Point& mouse_location,
      const std::vector<ui::AXEvent>& events) override {
    for (const ui::AXTreeUpdate& update : updates) {
      if (!ax_tree_.Unserialize(update)) {
        LOG(ERROR) << ax_tree_.error();
        FAIL() << "Could not unserialize AXTreeUpdate";
      }

      if (node_id_to_wait_for_ != ui::kInvalidAXNodeID) {
        int focused_node_id = update.tree_data.focus_id;
        focused_node_ids_.push_back(focused_node_id);
        if (focused_node_id == node_id_to_wait_for_) {
          node_id_to_wait_for_ = ui::kInvalidAXNodeID;
          run_loop_->Quit();
        }
      }
    }

    if (event_type_to_wait_for_ == ax::mojom::Event::kNone)
      return;

    for (const ui::AXEvent& event : events) {
      if (event.event_type == event_type_to_wait_for_ &&
          (event_target_node_id_to_wait_for_ == ui::kInvalidAXNodeID ||
           event_target_node_id_to_wait_for_ == event.id)) {
        matched_wait_for_event_ = std::make_unique<ui::AXEvent>(event);
        event_type_to_wait_for_ = ax::mojom::Event::kNone;
        run_loop_->Quit();
      }
    }
  }
  void DispatchAccessibilityLocationChange(
      const ui::AXTreeID& tree_id,
      const ui::AXLocationChange& details) override {}
  void DispatchTreeDestroyedEvent(ui::AXTreeID tree_id) override {}
  void DispatchActionResult(
      const ui::AXActionData& data,
      bool result,
      content::BrowserContext* browser_context = nullptr) override {}
  void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const std::optional<gfx::Rect>& rect) override {}

  ui::AXTree ax_tree_;
  std::unique_ptr<base::RunLoop> run_loop_;
  ui::AXNodeID node_id_to_wait_for_ = ui::kInvalidAXNodeID;
  std::vector<int> focused_node_ids_;

  std::unique_ptr<ui::AXEvent> matched_wait_for_event_;
  ax::mojom::Event event_type_to_wait_for_ = ax::mojom::Event::kNone;
  ui::AXNodeID event_target_node_id_to_wait_for_ = ui::kInvalidAXNodeID;
};

ui::TableColumn TestTableColumn(int id, const std::string& title) {
  ui::TableColumn column;
  column.id = id;
  column.title = base::ASCIIToUTF16(title.c_str());
  column.sortable = true;
  return column;
}

}  // namespace

typedef InProcessBrowserTest AutomationManagerAuraBrowserTest;

// A WebContents can be "hooked up" to the Chrome OS Desktop accessibility
// tree two different ways: via its aura::Window, and via a views::WebView.
// This test makes sure that we don't hook up both simultaneously, leading
// to the same web page appearing in the overall tree twice.
IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest, WebAppearsOnce) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);

  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  manager->Enable();
  auto* tree = manager->tree_.get();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL(
          "data:text/html;charset=utf-8,<button autofocus>Click me</button>")));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  WaitForAccessibilityTreeToContainNodeWithName(web_contents, "Click me");

  auto* frame_host = web_contents->GetPrimaryMainFrame();
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
    }
  }
}

IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest,
                       TransientFocusChangesAreSuppressed) {
  auto cache = std::make_unique<views::AXAuraObjCache>();
  auto* cache_ptr = cache.get();
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  manager->set_ax_aura_obj_cache_for_testing(std::move(cache));
  manager->send_window_state_on_enable_ = false;
  manager->Enable();
  AutomationEventWaiter waiter;

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = {0, 0, 200, 200};
  widget->Init(std::move(params));
  widget->Show();
  widget->Activate();

  cache_ptr->set_focused_widget_for_testing(widget);

  views::View* view1 = new views::View();
  view1->GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  view1->GetViewAccessibility().SetName("view1",
                                        ax::mojom::NameFrom::kAttribute);
  view1->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  widget->GetRootView()->AddChildView(view1);
  views::AXAuraObjWrapper* wrapper1 = cache_ptr->GetOrCreate(view1);
  views::View* view2 = new views::View();
  view2->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  view2->GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  view2->GetViewAccessibility().SetName("view2",
                                        ax::mojom::NameFrom::kAttribute);
  widget->GetRootView()->AddChildView(view2);
  views::AXAuraObjWrapper* wrapper2 = cache_ptr->GetOrCreate(view2);
  views::View* view3 = new views::View();
  view3->GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  view3->GetViewAccessibility().SetName("view3",
                                        ax::mojom::NameFrom::kAttribute);
  view3->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  widget->GetRootView()->AddChildView(view3);
  views::AXAuraObjWrapper* wrapper3 = cache_ptr->GetOrCreate(view3);

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

  RunAccessibilityChecks(widget);
}

// TODO(crbug.com/40179066): Crashes on Ozone.
#if BUILDFLAG(IS_OZONE)
#define MAYBE_ScrollView DISABLED_ScrollView
#else
#define MAYBE_ScrollView ScrollView
#endif
IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest, MAYBE_ScrollView) {
  auto cache = std::make_unique<views::AXAuraObjCache>();
  auto* cache_ptr = cache.get();
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  manager->set_ax_aura_obj_cache_for_testing(std::move(cache));
  manager->send_window_state_on_enable_ = false;
  manager->Enable();
  AutomationEventWaiter waiter;
  auto* tree = manager->tree_.get();

  // Create a widget with size 200, 200.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
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

  // Scroll right and check a scroll event occurred and the X position.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kScrollRight;
  scroll_view_wrapper->HandleAccessibleAction(action_data);
  auto event_from_views =
      waiter.WaitForEvent(ax::mojom::Event::kScrollPositionChanged);
  ASSERT_NE(nullptr, event_from_views.get());
  EXPECT_EQ(ax::mojom::EventFrom::kNone, event_from_views->event_from);
  tree->SerializeNode(scroll_view_wrapper, &node_data);
  EXPECT_NEAR(200, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollX),
              kAllowedError);

  // Scroll down and check a scroll event occurred and the Y position.
  action_data.action = ax::mojom::Action::kScrollDown;
  scroll_view_wrapper->HandleAccessibleAction(action_data);
  event_from_views =
      waiter.WaitForEvent(ax::mojom::Event::kScrollPositionChanged);
  ASSERT_NE(nullptr, event_from_views.get());
  EXPECT_EQ(ax::mojom::EventFrom::kNone, event_from_views->event_from);
  tree->SerializeNode(scroll_view_wrapper, &node_data);
  EXPECT_NEAR(200, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollY),
              kAllowedError);

  // Scroll to a specific location.
  action_data.action = ax::mojom::Action::kSetScrollOffset;
  action_data.target_point.SetPoint(50, 315);
  scroll_view_wrapper->HandleAccessibleAction(action_data);
  event_from_views =
      waiter.WaitForEvent(ax::mojom::Event::kScrollPositionChanged);
  ASSERT_NE(nullptr, event_from_views.get());
  EXPECT_EQ(ax::mojom::EventFrom::kNone, event_from_views->event_from);
  tree->SerializeNode(scroll_view_wrapper, &node_data);
  EXPECT_EQ(50, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollX));
  EXPECT_EQ(315, node_data.GetIntAttribute(ax::mojom::IntAttribute::kScrollY));
}

// Ensure that TableView accessibility works at the level of the
// serialized accessibility tree generated by AutomationManagerAura.
// TODO(crbug.com/40179066): Crashes on Ozone.
#if BUILDFLAG(IS_OZONE)
#define MAYBE_TableView DISABLED_TableView
#else
#define MAYBE_TableView TableView
#endif
IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest, MAYBE_TableView) {
  // Make our own AXAuraObjCache.
  auto cache = std::make_unique<views::AXAuraObjCache>();
  auto* cache_ptr = cache.get();
  ASSERT_NE(nullptr, cache_ptr);

  // Get the AutomationManagerAura and make it use our cache.
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  manager->set_ax_aura_obj_cache_for_testing(std::move(cache));
  manager->send_window_state_on_enable_ = false;
  manager->Enable();
  AutomationEventWaiter waiter;

  // Create a widget and give it explicit bounds that aren't at the top-left
  // of the screen so that we can check that the global bounding rect of
  // various accessibility nodes is correct.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  constexpr int kLeft = 100;
  constexpr int kTop = 500;
  constexpr int kWidth = 300;
  constexpr int kHeight = 200;
  params.bounds = {kLeft, kTop, kWidth, kHeight};
  widget->Init(std::move(params));

  // Construct a simple table model for testing, and then make a TableView
  // based on that. There are 4 columns specified here, and 4 rows
  // (TestTableModel fills cells in with fake data automatically).
  std::vector<ui::TableColumn> columns;
  columns.push_back(TestTableColumn(0, "Fruit"));
  columns.push_back(TestTableColumn(1, "Color"));
  columns.push_back(TestTableColumn(2, "Origin"));
  columns.push_back(TestTableColumn(3, "Price"));
  auto model = std::make_unique<TestTableModel>(4);  // Create 4 rows.

  // Add the TableView to our Widget's root view and give it bounds.
  // WARNING: This holds a raw pointer to `model`. To ensure the table doesn't
  // outlive its model, it must be manually deleted at the bottom of this
  // function.
  views::TableView* const table =
      widget->GetRootView()->AddChildView(std::make_unique<views::TableView>(
          model.get(), columns, views::TableType::kTextOnly,
          /* single_selection = */ true));
  table->SetBounds(0, 0, 200, 200);

  // Show the widget.
  widget->Show();
  widget->Activate();

  // Find the virtual views for the first row and first cell within the
  // table.
  ASSERT_EQ(4U, table->GetViewAccessibility().virtual_children().size());
  views::AXVirtualView* ax_row_0 =
      table->GetViewAccessibility().virtual_children()[0].get();
  ASSERT_EQ(4U, ax_row_0->children().size());
  views::AXVirtualView* ax_cell_0_0 = ax_row_0->children()[0].get();
  ASSERT_NE(nullptr, ax_cell_0_0);

  // This is the key part! Tell the table to focus and select the first row.
  // Then wait for an accessibility focus event on the first cell in the table!
  views::AXAuraObjWrapper* ax_cell_0_0_wrapper =
      ax_cell_0_0->GetOrCreateWrapper(cache_ptr);
  table->RequestFocus();
  table->Select(0);
  waiter.WaitForNodeIdToBeFocused(ax_cell_0_0_wrapper->GetUniqueId());

  // If we got this far, that means we got a focus event on the correct
  // node in the table. Now, find that same node in the AXTree (which is
  // after being serialized and unserialized) and ensure that the resulting
  // bounding box for the table cell is correct.
  {
    ui::AXNode* cell =
        waiter.ax_tree()->GetFromId(ax_cell_0_0_wrapper->GetUniqueId());
    ASSERT_TRUE(cell);
    EXPECT_EQ(ax::mojom::Role::kGridCell, cell->GetRole());
    gfx::RectF cell_bounds = waiter.ax_tree()->GetTreeBounds(cell);
    SCOPED_TRACE("Cell: " + cell_bounds.ToString());

    ui::AXNode* window = cell->parent();
    while (window && window->GetRole() != ax::mojom::Role::kWindow)
      window = window->parent();
    ASSERT_TRUE(window);

    gfx::RectF window_bounds = waiter.ax_tree()->GetTreeBounds(window);
    SCOPED_TRACE("Window: " + window_bounds.ToString());
    SCOPED_TRACE(waiter.ax_tree()->ToString());

    // The cell should have the same x, y as the window (with 1 pixel of slop).
    // The cell should have a width that's less than or equal to the window
    // width, and the cell's height should be significantly smaller so we
    // assert that the cell's height is greater than zero, but less than half
    // the window height.
    EXPECT_NEAR(cell_bounds.x(), window_bounds.x(), 1);
    EXPECT_NEAR(cell_bounds.y(), window_bounds.y(), 1);
    EXPECT_LE(cell_bounds.width(), window_bounds.width());
    EXPECT_GT(cell_bounds.height(), 0);
    EXPECT_LT(cell_bounds.height(), window_bounds.height() / 2);
  }
  // Remove and destroy the TableView, it refers to `model` which is about to go
  // out of scope.
  widget->GetRootView()->RemoveChildViewT(table);
}

IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest, EventFromAction) {
  auto cache = std::make_unique<views::AXAuraObjCache>();
  auto* cache_ptr = cache.get();
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  manager->send_window_state_on_enable_ = false;
  manager->set_ax_aura_obj_cache_for_testing(std::move(cache));
  manager->Enable();
  AutomationEventWaiter waiter;

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = {0, 0, 200, 200};
  widget->Init(std::move(params));
  widget->Show();
  widget->Activate();

  cache_ptr->set_focused_widget_for_testing(widget);

  views::View* view1 = new views::View();
  view1->GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  view1->GetViewAccessibility().SetName("view1",
                                        ax::mojom::NameFrom::kAttribute);
  view1->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  widget->GetRootView()->AddChildView(view1);
  views::View* view2 = new views::View();
  view2->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  view2->GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  view2->GetViewAccessibility().SetName("view2",
                                        ax::mojom::NameFrom::kAttribute);
  widget->GetRootView()->AddChildView(view2);
  views::AXAuraObjWrapper* wrapper2 = cache_ptr->GetOrCreate(view2);

  // Focus view1, simulating the non-accessibility action, block until we get an
  // accessibility event that shows this view is focused.
  view1->RequestFocus();
  auto event_from_views = waiter.WaitForEvent(
      ax::mojom::Event::kFocus, view1->GetViewAccessibility().GetUniqueId());
  ASSERT_NE(nullptr, event_from_views.get());
  EXPECT_EQ(ax::mojom::EventFrom::kNone, event_from_views->event_from);

  // Focus view2, simulating the accessibility action, block until we get an
  // accessibility event that shows this view is focused.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  action_data.target_tree_id = manager->tree_.get()->tree_id();
  action_data.target_node_id = wrapper2->GetUniqueId();

  manager->PerformAction(action_data);
  auto event_from_action = waiter.WaitForEvent(
      ax::mojom::Event::kFocus, view2->GetViewAccessibility().GetUniqueId());
  ASSERT_NE(nullptr, event_from_action.get());
  EXPECT_EQ(ax::mojom::EventFrom::kAction, event_from_action->event_from);

  cache_ptr->set_focused_widget_for_testing(nullptr);

  RunAccessibilityChecks(widget);
}

// Verify that re-enabling AutomationManagerAura after disable will not cause
// crash.  See https://crbug.com/1177042.
IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest,
                       ReenableDoesNotCauseCrash) {
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();
  manager->Enable();

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = {0, 0, 200, 200};
  widget->Init(std::move(params));
  widget->Show();
  widget->Activate();

  manager->Disable();

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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

IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest, GetFocusOnChildTree) {
  views::AXAuraObjCache cache;
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = {0, 0, 200, 200};
  widget->Init(std::move(params));
  widget->Show();
  widget->Activate();

  cache.set_focused_widget_for_testing(widget);

  // No focus falls back on root view.
  EXPECT_EQ(cache.GetOrCreate(widget->GetRootView()), cache.GetFocus());

  // A child of the client view results in a focus if it has a tree id.
  views::View* child =
      widget->non_client_view()->client_view()->children().front();
  ASSERT_NE(nullptr, child);

  // No tree id yet.
  EXPECT_EQ(cache.GetOrCreate(widget->GetRootView()), cache.GetFocus());

  // Now, there's a tree id.
  child->GetViewAccessibility().SetChildTreeID(
      ui::AXTreeID::CreateNewAXTreeID());
  EXPECT_EQ(cache.GetOrCreate(child), cache.GetFocus());

  cache.set_focused_widget_for_testing(nullptr);

  RunAccessibilityChecks(widget);
}

IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest, ObservedOnEnable) {
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();

  // Before enabling, we should not be listening to AutomationEventRouter.
  EXPECT_FALSE(
      extensions::AutomationEventRouter::GetInstance()->HasObserver(manager));

  // After enabling, we should be observing.
  manager->Enable();
  EXPECT_TRUE(
      extensions::AutomationEventRouter::GetInstance()->HasObserver(manager));

  manager->Disable();
  EXPECT_FALSE(
      extensions::AutomationEventRouter::GetInstance()->HasObserver(manager));
}

IN_PROC_BROWSER_TEST_F(AutomationManagerAuraBrowserTest,
                       CallsWhenDisabledDoNotCrash) {
  AutomationManagerAura* manager = AutomationManagerAura::GetInstance();

  // Call some methods while disabled, before the first enable.
  manager->HandleEvent(ax::mojom::Event::kFocus, /*from_user=*/true);
  manager->HandleAlert("hello");
  manager->PerformAction(ui::AXActionData());
  manager->OnChildWindowRemoved(nullptr);

  // Flip things on then immediately off.
  manager->Enable();
  manager->Disable();

  // Make the same calls again. We should never crash.
  manager->HandleEvent(ax::mojom::Event::kFocus, /*from_user=*/true);
  manager->HandleAlert("hello");
  manager->PerformAction(ui::AXActionData());
  manager->OnChildWindowRemoved(nullptr);

  manager->HandleEvent(ax::mojom::Event::kMouseMoved, /*from_user=*/false);
  manager->HandleAlert("hello");
  manager->PerformAction(ui::AXActionData());
  manager->OnChildWindowRemoved(nullptr);
}
