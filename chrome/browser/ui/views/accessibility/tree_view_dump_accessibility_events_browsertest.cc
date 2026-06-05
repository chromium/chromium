// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/build_config.h"
#include "chrome/browser/ui/views/accessibility/dump_accessibility_events_views_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/base/models/tree_node_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

using TestNode = ui::TreeNodeWithValue<int>;

class TreeViewDumpAccessibilityEventsTest
    : public DumpAccessibilityEventsViewsTestBase {
 public:
  std::vector<ui::AXPropertyFilter> DefaultFilters() const override {
    std::vector<ui::AXPropertyFilter> filters;

#if BUILDFLAG(IS_WIN)
    filters.emplace_back("ExpandCollapseExpandCollapseState*",
                         ui::AXPropertyFilter::ALLOW);
    filters.emplace_back("StructureChanged*", ui::AXPropertyFilter::DENY);
    filters.emplace_back("AriaProperties*", ui::AXPropertyFilter::DENY);
#elif BUILDFLAG(IS_MAC)
    filters.emplace_back("AXRowExpanded*", ui::AXPropertyFilter::ALLOW);
    filters.emplace_back("AXRowCollapsed*", ui::AXPropertyFilter::ALLOW);
    // ViewsAXEnabled fires AXRowCountChanged (via AXEventGenerator) in addition
    // to AXRowExpanded/AXRowCollapsed. ViewsAXDisabled does not. Suppress the
    // extra event so both variants produce the same output.
    filters.emplace_back("AXRowCountChanged*", ui::AXPropertyFilter::DENY);
#endif

    return filters;
  }

  void SetUpTestViews() override {
    auto container = std::make_unique<View>();
    container->SetLayoutManager(std::make_unique<FillLayout>());

    auto root = std::make_unique<TestNode>(u"Root", 0);
    auto child1 = std::make_unique<TestNode>(u"Child 1", 1);
    auto child2 = std::make_unique<TestNode>(u"Child 2", 2);
    // A grandchild makes child1 expandable.
    child1->Add(std::make_unique<TestNode>(u"Grandchild 1", 3), 0);
    root->Add(std::move(child1), 0);
    root->Add(std::move(child2), 1);

    model_ = std::make_unique<ui::TreeNodeModel<TestNode>>(std::move(root));

    auto tree_view = std::make_unique<TreeView>();
    tree_view->SetModel(model_.get());
    tree_view->GetViewAccessibility().SetName(u"Test Tree");
    tree_view_ = container->AddChildView(std::move(tree_view));

    widget()->SetContentsView(std::move(container));
    widget()->Show();
  }

  void TearDownOnMainThread() override {
    if (tree_view_) {
      tree_view_->SetModel(nullptr);
    }
    tree_view_ = nullptr;
    model_.reset();
    DumpAccessibilityEventsViewsTestBase::TearDownOnMainThread();
  }

 protected:
  raw_ptr<TreeView> tree_view_ = nullptr;
  std::unique_ptr<ui::TreeNodeModel<TestNode>> model_;
};

// TODO(crbug.com/468203351): Re-enable when no longer flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ExpandNode DISABLED_ExpandNode
#else
#define MAYBE_ExpandNode ExpandNode
#endif
IN_PROC_BROWSER_TEST_P(TreeViewDumpAccessibilityEventsTest, MAYBE_ExpandNode) {
#if BUILDFLAG(IS_LINUX)
  if (IsViewsAXEnabled()) {
    GTEST_SKIP() << "crbug.com/40672441";
  }
#endif

  AddAllowFilter("STATE-CHANGE:EXPANDED:TRUE*");
  ui::TreeModelNode* child1 = model_->GetRoot()->children()[0].get();
  BEGIN_RECORDING_EVENTS_OR_SKIP("tree-expand-node");
  tree_view_->Expand(child1);
  WaitForPendingSerialization();
}

// TODO(crbug.com/468203351): Re-enable when no longer flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_CollapseNode DISABLED_CollapseNode
#else
#define MAYBE_CollapseNode CollapseNode
#endif
IN_PROC_BROWSER_TEST_P(TreeViewDumpAccessibilityEventsTest,
                       MAYBE_CollapseNode) {
  AddAllowFilter("STATE-CHANGE:EXPANDED:FALSE*");
  ui::TreeModelNode* child1 = model_->GetRoot()->children()[0].get();
  tree_view_->Expand(child1);
  WaitForPendingSerialization();

  BEGIN_RECORDING_EVENTS_OR_SKIP("tree-collapse-node");
  tree_view_->Collapse(child1);
}

// Verifies that selecting a tree node fires the appropriate platform selection
// events. TreeView::SetSelectedNode() calls SetIsSelected(true) on the node's
// AXVirtualView, which fires Event::kSelection through ViewAccessibility.
IN_PROC_BROWSER_TEST_P(TreeViewDumpAccessibilityEventsTest, SelectNode) {
  SetFilters(R"(
@WIN-ALLOW:EVENT_OBJECT_SELECTION on*
@UIA-WIN-ALLOW:SelectionItem_ElementSelected*
@AURALINUX-ALLOW:STATE-CHANGE:SELECTED*
@AURALINUX-ALLOW:SELECTION-CHANGED*
)");
  ui::TreeModelNode* child2 = model_->GetRoot()->children()[1].get();
  const std::string test_name =
      IsViewsAXEnabled() ? "tree-select-node-viewsax" : "tree-select-node";
  BEGIN_RECORDING_EVENTS_OR_SKIP(test_name);
  tree_view_->SetSelectedNode(child2);
  WaitForPendingSerialization();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TreeViewDumpAccessibilityEventsTest,
    ::testing::ValuesIn(
        DumpAccessibilityEventsViewsTestBase::EventTestPasses()),
    EventTestPassToString());

}  // namespace
}  // namespace views
