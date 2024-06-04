// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "components/ui_devtools/views/dom_agent_views.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/css_agent.h"
#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/views/overlay_agent_views.h"
#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "components/ui_devtools/views/window_element.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#endif  // defined(USE_AURA)

namespace ui_devtools {

namespace DOM = protocol::DOM;

namespace {

class TestView : public views::View {
  METADATA_HEADER(TestView, views::View)

 public:
  TestView(const std::string name) : name_(name) {}

  TestView(const TestView&) = delete;
  TestView& operator=(const TestView&) = delete;

  std::string GetObjectName() const override { return name_; }

 private:
  const std::string name_;
};

BEGIN_METADATA(TestView)
END_METADATA

std::string GetAttributeValue(const std::string& attribute, DOM::Node* node) {
  EXPECT_TRUE(node->hasAttributes());
  protocol::Array<std::string>* attributes = node->getAttributes(nullptr);
  for (size_t i = 0; i < attributes->size() - 1; i += 2) {
    if ((*attributes)[i] == attribute)
      return (*attributes)[i + 1];
  }
  return std::string();
}

DOM::Node* FindNodeWithID(int id, DOM::Node* root) {
  if (id == root->getNodeId()) {
    return root;
  }
  protocol::Array<DOM::Node>* children = root->getChildren(nullptr);
  for (size_t i = 0; i < children->size(); i++) {
    if (DOM::Node* node = FindNodeWithID(id, (*children)[i].get()))
      return node;
  }
  return nullptr;
}

}  // namespace

class DOMAgentTest : public views::ViewsTestBase {
 public:
  DOMAgentTest() = default;

  DOMAgentTest(const DOMAgentTest&) = delete;
  DOMAgentTest& operator=(const DOMAgentTest&) = delete;

  ~DOMAgentTest() override = default;

  views::Widget::InitParams CreateParams(
      views::Widget::InitParams::Ownership ownership,
      views::Widget::InitParams::Type type) override {
    views::Widget::InitParams params =
        views::ViewsTestBase::CreateParams(ownership, type);
    params.name = name_;
    return params;
  }

  views::internal::NativeWidgetPrivate* CreateTestNativeWidget() {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
#if defined(USE_AURA)
    params.parent = GetContext();
#endif
    widget->Init(std::move(params));
    return widget->native_widget_private();
  }

  std::unique_ptr<views::Widget> CreateNamedWidget(const std::string& name) {
    name_ = name;
    std::unique_ptr<views::Widget> widget =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    name_.clear();
    widget->Show();
    return widget;
  }

#if defined(USE_AURA)
  std::unique_ptr<aura::Window> CreateChildWindow(
      aura::Window* parent,
      aura::client::WindowType type = aura::client::WINDOW_TYPE_NORMAL) {
    std::unique_ptr<aura::Window> window =
        std::make_unique<aura::Window>(nullptr, type);
    window->Init(ui::LAYER_NOT_DRAWN);
    window->SetBounds(gfx::Rect());
    parent->AddChild(window.get());
    window->Show();
    return window;
  }
#endif

  void SetUp() override {
    fake_frontend_channel_ = std::make_unique<FakeFrontendChannel>();
    uber_dispatcher_ = std::make_unique<protocol::UberDispatcher>(
        fake_frontend_channel_.get());
    dom_agent_ = DOMAgentViews::Create();
    dom_agent_->Init(uber_dispatcher_.get());
    css_agent_ = std::make_unique<CSSAgent>(dom_agent_.get());
    css_agent_->Init(uber_dispatcher_.get());
    css_agent_->enable();
    overlay_agent_ = OverlayAgentViews::Create(dom_agent_.get());
    overlay_agent_->Init(uber_dispatcher_.get());
    overlay_agent_->enable();

    // We need to create |dom_agent| first to observe creation of
    // WindowTreeHosts in ViewTestBase::SetUp().
    views::ViewsTestBase::SetUp();

#if defined(USE_AURA)
    top_window = CreateChildWindow(GetContext());
#endif
  }

  void TearDown() override {
#if defined(USE_AURA)
    top_window.reset();
#endif
    css_agent_.reset();
    overlay_agent_.reset();
    dom_agent_.reset();

    uber_dispatcher_.reset();
    fake_frontend_channel_.reset();
    views::ViewsTestBase::TearDown();
  }

  template <typename T>
  bool WasChildNodeInserted(T* parent, int previous_sibling_id = 0) {
    const int parent_id = GetIDForBackendElement(parent);
    return frontend_channel()->CountProtocolNotificationMessageStartsWith(
               base::StringPrintf("{\"method\":\"DOM.childNodeInserted\","
                                  "\"params\":{\"parentNodeId\":%d,"
                                  "\"previousNodeId\":%d",
                                  parent_id, previous_sibling_id)) == 1;
  }
  template <typename T, typename U>
  bool WasChildNodeInserted(T* parent, U* previous_sibling) {
    const int previous_sibling_id =
        previous_sibling ? GetIDForBackendElement(previous_sibling) : 0;
    return WasChildNodeInserted(parent, previous_sibling_id);
  }

  template <typename T>
  bool WasChildNodeRemoved(T* parent, int node_id) {
    const int parent_id = GetIDForBackendElement(parent);
    return frontend_channel()->CountProtocolNotificationMessage(
               base::StringPrintf(
                   "{\"method\":\"DOM.childNodeRemoved\",\"params\":{"
                   "\"parentNodeId\":%d,\"nodeId\":%d}}",
                   parent_id, node_id)) == 1;
  }

  template <typename T>
  int GetIDForBackendElement(T* element) {
    return dom_agent()->element_root()->FindUIElementIdForBackendElement<T>(
        element);
  }

  template <typename T>
  DOM::Node* FindInRoot(T* element, DOM::Node* root) {
    return FindNodeWithID(GetIDForBackendElement(element), root);
  }

  // The following three methods test that a tree of DOM::Node exactly
  // corresponds 1:1 with a views/Aura hierarchy

#if defined(USE_AURA)
  bool ElementTreeMatchesDOMTree(aura::Window* window, DOM::Node* root) {
    if (GetIDForBackendElement(window) != root->getNodeId() ||
        "Window" != root->getNodeName() ||
        window->GetName() != GetAttributeValue("name", root)) {
      return false;
    }

    protocol::Array<DOM::Node>* children = root->getChildren(nullptr);
    size_t child_index = 0;
    views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
    if (widget &&
        !ElementTreeMatchesDOMTree(widget, (*children)[child_index++].get())) {
      return false;
    }
    for (aura::Window* child_window : window->children()) {
      if (!ElementTreeMatchesDOMTree(child_window,
                                     (*children)[child_index++].get()))
        return false;
    }
    // Make sure there are no stray children.
    return child_index == children->size();
  }
#endif

  bool ElementTreeMatchesDOMTree(views::Widget* widget, DOM::Node* root) {
    if (GetIDForBackendElement(widget) != root->getNodeId() ||
        "Widget" != root->getNodeName() ||
        widget->GetName() != GetAttributeValue("name", root)) {
      return false;
    }

    protocol::Array<DOM::Node>* children = root->getChildren(nullptr);
    views::View* root_view = widget->GetRootView();
    return root_view
               ? ElementTreeMatchesDOMTree(root_view, (*children)[0].get())
               : children->empty();
  }

  bool ElementTreeMatchesDOMTree(views::View* view, DOM::Node* root) {
    if (GetIDForBackendElement(view) != root->getNodeId() ||
        "View" != root->getNodeName() ||
        view->GetObjectName() != GetAttributeValue("name", root)) {
      return false;
    }

    protocol::Array<DOM::Node>* children = root->getChildren(nullptr);
    std::vector<raw_ptr<views::View, VectorExperimental>> child_views =
        view->GetChildrenInZOrder();
    const size_t child_count = child_views.size();
    if (child_count != children->size())
      return false;

    for (size_t i = 0; i < child_count; i++) {
      if (!ElementTreeMatchesDOMTree(child_views[i], (*children)[i].get()))
        return false;
    }
    return true;
  }

  FakeFrontendChannel* frontend_channel() {
    return fake_frontend_channel_.get();
  }
  DOMAgentViews* dom_agent() { return dom_agent_.get(); }

#if defined(USE_AURA)
  std::unique_ptr<aura::Window> top_window;
#endif
 private:
  std::unique_ptr<protocol::UberDispatcher> uber_dispatcher_;
  std::unique_ptr<FakeFrontendChannel> fake_frontend_channel_;
  std::unique_ptr<DOMAgentViews> dom_agent_;
  std::unique_ptr<CSSAgent> css_agent_;
  std::unique_ptr<OverlayAgentViews> overlay_agent_;
  std::string name_;
};

// Tests to ensure that the DOMAgent's hierarchy matches the real hierarchy.
#if defined(USE_AURA)

TEST_F(DOMAgentTest, GetDocumentWithWindowWidgetView) {
  // parent_window
  //   widget
  //     (root/content views)
  //        child_view
  //   child_window
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  aura::Window* parent_window = widget->GetNativeWindow();
  parent_window->SetName("parent_window");
  std::unique_ptr<aura::Window> child_window = CreateChildWindow(parent_window);
  child_window->SetName("child_window");
  widget->Show();
  widget->GetRootView()->AddChildView(std::make_unique<TestView>("child_view"));

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* parent_node = FindInRoot(parent_window, root.get());
  DCHECK(parent_node);

  EXPECT_TRUE(ElementTreeMatchesDOMTree(parent_window, parent_node));
}

TEST_F(DOMAgentTest, GetDocumentNativeWidgetOwnsWidget) {
  views::internal::NativeWidgetPrivate* native_widget_private =
      CreateTestNativeWidget();
  views::Widget* widget = native_widget_private->GetWidget();
  aura::Window* parent_window = widget->GetNativeWindow();

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* parent_node = FindInRoot(parent_window, root.get());
  DCHECK(parent_node);

  EXPECT_TRUE(ElementTreeMatchesDOMTree(parent_window, parent_node));
  // Destroy NativeWidget followed by |widget|
  widget->CloseNow();
}

#endif  // defined(USE_AURA)

TEST_F(DOMAgentTest, GetDocumentMultipleWidgets) {
  // widget_a
  //   (root/contents views)
  //     child_a1
  //     child_a2
  // widget_b
  //   (root/contents views)
  //      child_b1
  //        child_b11
  //          child_b111
  //            child_b1111
  //          child_b112
  //          child_b113
  //        child_b12
  //          child_b121
  //          child_b122

  std::unique_ptr<views::Widget> widget_a =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  std::unique_ptr<views::Widget> widget_b =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget_a->GetRootView()->AddChildView(std::make_unique<TestView>("child_a1"));
  widget_a->GetRootView()->AddChildView(std::make_unique<TestView>("child_a2"));

  {
    auto child_b111 = std::make_unique<TestView>("child_b111");
    child_b111->AddChildView(std::make_unique<TestView>("child_b1111"));

    auto child_b11 = std::make_unique<TestView>("child_b11");
    child_b11->AddChildView(std::move(child_b111));
    child_b11->AddChildView(std::make_unique<TestView>("child_b112"));
    child_b11->AddChildView(std::make_unique<TestView>("child_b113"));

    auto child_b12 = std::make_unique<TestView>("child_b12");
    child_b12->AddChildView(std::make_unique<TestView>("child_b121"));
    child_b12->AddChildView(std::make_unique<TestView>("child_b122"));

    auto child_b1 = std::make_unique<TestView>("child_b1");
    child_b1->AddChildView(std::move(child_b11));
    child_b1->AddChildView(std::move(child_b12));

    widget_b->GetRootView()->AddChildView(std::move(child_b1));
  }

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* widget_node_a = FindInRoot(widget_a.get(), root.get());
  DCHECK(widget_node_a);

  EXPECT_TRUE(ElementTreeMatchesDOMTree(widget_a.get(), widget_node_a));

  DOM::Node* widget_node_b = FindInRoot(widget_b.get(), root.get());
  DCHECK(widget_node_b);

  EXPECT_TRUE(ElementTreeMatchesDOMTree(widget_b.get(), widget_node_b));
}

// Tests to ensure correct messages are sent when elements are added,
// removed, and moved around.

#if defined(USE_AURA)
TEST_F(DOMAgentTest, WindowAddedChildNodeInserted) {
  // Initialize DOMAgent
  std::unique_ptr<aura::Window> first_child =
      CreateChildWindow(top_window.get());

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  std::unique_ptr<aura::Window> second_child(
      CreateChildWindow(top_window.get()));
  EXPECT_TRUE(WasChildNodeInserted(top_window.get(), first_child.get()));
}

TEST_F(DOMAgentTest, WindowDestroyedChildNodeRemoved) {
  std::unique_ptr<aura::Window> child_1 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_2 = CreateChildWindow(child_1.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int child_id = GetIDForBackendElement(child_2.get());

  child_2.reset();
  EXPECT_TRUE(WasChildNodeRemoved(child_1.get(), child_id));
}

TEST_F(DOMAgentTest, WindowReorganizedChildNodeRearranged) {
  std::unique_ptr<aura::Window> child_1 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_2 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_11 = CreateChildWindow(child_1.get());
  std::unique_ptr<aura::Window> child_21 = CreateChildWindow(child_2.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int moving_child_id = GetIDForBackendElement(child_11.get());
  child_2->AddChild(child_11.get());
  EXPECT_TRUE(WasChildNodeRemoved(child_1.get(), moving_child_id));
  EXPECT_TRUE(WasChildNodeInserted(child_2.get(), child_21.get()));
}

TEST_F(DOMAgentTest, WindowReorganizedChildNodeRemovedAndInserted) {
  std::unique_ptr<aura::Window> child_1 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_2 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_21 = CreateChildWindow(child_2.get());
  std::unique_ptr<aura::Window> child_22 = CreateChildWindow(child_2.get());
  // Initialized at the end since it will be a child of |child_2| at
  // tear down.
  std::unique_ptr<aura::Window> child_11 = CreateChildWindow(child_1.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int moving_child_id = GetIDForBackendElement(child_11.get());

  child_1->RemoveChild(child_11.get());
  EXPECT_TRUE(WasChildNodeRemoved(child_1.get(), moving_child_id));

  child_2->AddChild(child_11.get());
  EXPECT_TRUE(WasChildNodeInserted(child_2.get(), child_22.get()));
}

TEST_F(DOMAgentTest, WindowStackingChangedChildNodeRemovedAndInserted) {
  std::unique_ptr<aura::Window> child_11 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_12 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_13 = CreateChildWindow(top_window.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int moving_child_id = GetIDForBackendElement(child_11.get());
  top_window->StackChildAbove(child_11.get(), child_12.get());
  EXPECT_TRUE(WasChildNodeRemoved(top_window.get(), moving_child_id));
  EXPECT_TRUE(WasChildNodeInserted(top_window.get(), child_12.get()));
}
#endif  // defined(USE_AURA)

TEST_F(DOMAgentTest, ViewInserted) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_WINDOW);
  widget->Show();

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  views::View* root_view = widget->GetRootView();
  ASSERT_FALSE(root_view->children().empty());
  auto* last_child = root_view->children().back().get();
  root_view->AddChildView(std::make_unique<views::View>());
  EXPECT_TRUE(WasChildNodeInserted(root_view, last_child));
}

TEST_F(DOMAgentTest, ViewRemoved) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->Show();
  views::View* root_view = widget->GetRootView();

  views::View* child_view =
      root_view->AddChildView(std::make_unique<views::View>());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int removed_node_id = GetIDForBackendElement(child_view);
  root_view->RemoveChildViewT(child_view);
  EXPECT_TRUE(WasChildNodeRemoved(root_view, removed_node_id));
}

TEST_F(DOMAgentTest, ViewRearranged) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  widget->Show();
  views::View* root_view = widget->GetRootView();
  views::View* parent_view =
      root_view->AddChildView(std::make_unique<views::View>());
  views::View* target_view =
      root_view->AddChildView(std::make_unique<views::View>());
  views::View* child_view =
      parent_view->AddChildView(std::make_unique<views::View>());
  views::View* child_view_1 =
      parent_view->AddChildView(std::make_unique<views::View>());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  // Reorder child_view_1 from index 1 to 0 in view::Views tree. This makes
  // DOM tree remove view node at position 1 and insert it at position 0.
  int child_1_id = GetIDForBackendElement(child_view_1);
  parent_view->ReorderChildView(child_view_1, 0);
  EXPECT_TRUE(WasChildNodeRemoved(parent_view, child_1_id));
  EXPECT_TRUE(WasChildNodeInserted(parent_view));

  // Reorder child_view_1 to the same index 0 shouldn't perform reroder
  // work, so we still expect 1 remove and 1 insert protocol notification
  // messages.
  parent_view->ReorderChildView(child_view_1, 0);
  EXPECT_TRUE(WasChildNodeRemoved(parent_view, child_1_id));
  EXPECT_TRUE(WasChildNodeInserted(parent_view));

  int child_id = GetIDForBackendElement(child_view);
  auto owned_child_view = parent_view->RemoveChildViewT(child_view);
  target_view->AddChildView(std::move(owned_child_view));
  EXPECT_TRUE(WasChildNodeRemoved(parent_view, child_id));
  EXPECT_TRUE(WasChildNodeInserted(target_view));
}

TEST_F(DOMAgentTest, ViewRearrangedRemovedAndInserted) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  widget->Show();
  views::View* root_view = widget->GetRootView();
  views::View* parent_view =
      root_view->AddChildView(std::make_unique<views::View>());
  views::View* target_view =
      root_view->AddChildView(std::make_unique<views::View>());
  views::View* child_view =
      parent_view->AddChildView(std::make_unique<views::View>());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int child_id = GetIDForBackendElement(child_view);
  auto owned_child_view = parent_view->RemoveChildViewT(child_view);
  target_view->AddChildView(std::move(owned_child_view));
  EXPECT_TRUE(WasChildNodeRemoved(parent_view, child_id));
  EXPECT_TRUE(WasChildNodeInserted(target_view));
}

TEST_F(DOMAgentTest, NodeIdToUIElementTest) {
  // widget
  //   root_view
  //     child_a1
  //       child_a11
  //         child_a111
  //           child_a1111
  //         child_a112
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  widget->Show();
  views::View* root_view = widget->GetRootView();
  views::View* child_a1 =
      root_view->AddChildView(std::make_unique<views::View>());
  views::View* child_a11 =
      child_a1->AddChildView(std::make_unique<views::View>());
  views::View* child_a111 =
      child_a11->AddChildView(std::make_unique<views::View>());
  views::View* child_a112 =
      child_a11->AddChildView(std::make_unique<views::View>());
  views::View* child_a1111 =
      child_a111->AddChildView(std::make_unique<views::View>());

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  int child_a1_id = GetIDForBackendElement(child_a1);
  int child_a11_id = GetIDForBackendElement(child_a11);
  int child_a111_id = GetIDForBackendElement(child_a111);
  int child_a112_id = GetIDForBackendElement(child_a112);
  int child_a1111_id = GetIDForBackendElement(child_a1111);

  // Make sure all child nodes are in the |node_id_to_ui_element_| map.
  EXPECT_NE(dom_agent()->GetElementFromNodeId(child_a1_id), nullptr);
  EXPECT_NE(dom_agent()->GetElementFromNodeId(child_a11_id), nullptr);
  EXPECT_NE(dom_agent()->GetElementFromNodeId(child_a111_id), nullptr);
  EXPECT_NE(dom_agent()->GetElementFromNodeId(child_a112_id), nullptr);
  EXPECT_NE(dom_agent()->GetElementFromNodeId(child_a1111_id), nullptr);

  root_view->RemoveChildView(child_a1);

  // Check that child_a1 and its children are all removed from the
  // |node_id_to_ui_element_| map.
  EXPECT_EQ(dom_agent()->GetElementFromNodeId(child_a1_id), nullptr);
  EXPECT_EQ(dom_agent()->GetElementFromNodeId(child_a11_id), nullptr);
  EXPECT_EQ(dom_agent()->GetElementFromNodeId(child_a111_id), nullptr);
  EXPECT_EQ(dom_agent()->GetElementFromNodeId(child_a112_id), nullptr);
  EXPECT_EQ(dom_agent()->GetElementFromNodeId(child_a1111_id), nullptr);

  // Required since it was removed from its parent view.
  delete child_a1;
}

// Tests to ensure dom search for native UI is working
TEST_F(DOMAgentTest, SimpleDomSearch) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->GetRootView()->AddChildView(std::make_unique<TestView>("child_a1"));
  widget->GetRootView()->AddChildView(std::make_unique<TestView>("child_a2"));

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  std::string search_id;
  int result_count = 0;
  std::unique_ptr<protocol::Array<int>> node_ids;

  // 1 match
  dom_agent()->performSearch("child_a1", false, &search_id, &result_count);
  EXPECT_EQ(result_count, 1);
  dom_agent()->getSearchResults(search_id, 0, result_count, &node_ids);
  ASSERT_TRUE(node_ids);
  EXPECT_EQ(node_ids->size(), 1u);
  dom_agent()->discardSearchResults(search_id);
  node_ids.reset();

  // no match
  dom_agent()->performSearch("child_a12", false, &search_id, &result_count);
  EXPECT_EQ(result_count, 0);
  dom_agent()->getSearchResults(search_id, 0, 1, &node_ids);
  EXPECT_TRUE(!node_ids);
}

TEST_F(DOMAgentTest, ExactDomSearch) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->GetRootView()->AddChildView(std::make_unique<TestView>("child_a"));
  widget->GetRootView()->AddChildView(std::make_unique<TestView>("child_aa"));

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);
  std::string search_id;
  int result_count = 0;
  std::unique_ptr<protocol::Array<int>> node_ids;

  // substring matches
  dom_agent()->performSearch("child_a", false, &search_id, &result_count);
  EXPECT_EQ(result_count, 2);
  dom_agent()->getSearchResults(search_id, 0, result_count, &node_ids);
  ASSERT_TRUE(node_ids);
  EXPECT_EQ(node_ids->size(), 2u);
  dom_agent()->discardSearchResults(search_id);
  node_ids.reset();

  // exact string matches
  dom_agent()->performSearch("\"child_a\"", false, &search_id, &result_count);
  EXPECT_EQ(result_count, 1);
  dom_agent()->getSearchResults(search_id, 0, result_count, &node_ids);
  ASSERT_TRUE(node_ids);
  EXPECT_EQ(node_ids->size(), 1u);
  dom_agent()->discardSearchResults(search_id);
  node_ids.reset();

  dom_agent()->performSearch("\"child\"", false, &search_id, &result_count);
  EXPECT_EQ(result_count, 0);
  dom_agent()->getSearchResults(search_id, 0, 1, &node_ids);
  EXPECT_TRUE(!node_ids);
}

TEST_F(DOMAgentTest, TagDomSearch) {
  std::string widget_name = "TestElement";
  std::unique_ptr<views::Widget> widget_a = CreateNamedWidget(widget_name);
  std::unique_ptr<views::Widget> widget_b = CreateNamedWidget(widget_name);
  std::unique_ptr<views::Widget> widget_c = CreateNamedWidget(widget_name);
  widget_a->GetRootView()->AddChildView(
      std::make_unique<TestView>("WidgetView"));

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);
  std::string search_id;
  int result_count = 0;
  std::unique_ptr<protocol::Array<int>> node_ids;

  // normal search looks for any "widget" substrings
  dom_agent()->performSearch("widget", false, &search_id, &result_count);
  EXPECT_EQ(result_count, 4);
  dom_agent()->getSearchResults(search_id, 0, result_count, &node_ids);
  ASSERT_TRUE(node_ids);
  EXPECT_EQ(node_ids->size(), 4u);
  dom_agent()->discardSearchResults(search_id);
  node_ids.reset();

  // tag search only looks for <widget...>
  dom_agent()->performSearch("<widget>", false, &search_id, &result_count);
  EXPECT_EQ(result_count, 3);
  dom_agent()->getSearchResults(search_id, 0, result_count, &node_ids);
  ASSERT_TRUE(node_ids);
  EXPECT_EQ(node_ids->size(), 3u);
}

TEST_F(DOMAgentTest, DomSearchForStylesPanel) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->GetRootView()->AddChildView(std::make_unique<TestView>("child_a1"));

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  std::string search_id;
  int result_count = 0;
  std::unique_ptr<protocol::Array<int>> node_ids;

  // Search for something that is in style properties but not in dom name or
  // attributes.
  dom_agent()->performSearch("style: name: child_a1", false, &search_id,
                             &result_count);
  EXPECT_EQ(result_count, 1);
  dom_agent()->getSearchResults(search_id, 0, result_count, &node_ids);
  ASSERT_TRUE(node_ids);
  EXPECT_EQ(node_ids->size(), 1u);
  node_ids.reset();

  dom_agent()->performSearch("name: child_a1", false, &search_id,
                             &result_count);
  EXPECT_EQ(result_count, 0);
  dom_agent()->getSearchResults(search_id, 0, 1, &node_ids);
  EXPECT_TRUE(!node_ids);
}

DECLARE_ELEMENT_IDENTIFIER_VALUE(kTestElementID);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kTestElementID);

TEST_F(DOMAgentTest, DomSearchForElementID) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->GetRootView()->AddChildView(
      views::Builder<views::View>()
          .SetProperty(views::kElementIdentifierKey, kTestElementID)
          .Build());

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  std::string search_id;
  int result_count = 0;
  std::unique_ptr<protocol::Array<int>> node_ids;

  // Match ID for View element.
  dom_agent()->performSearch("id: kTestElementID", false, &search_id,
                             &result_count);
  EXPECT_EQ(result_count, 1);
  dom_agent()->getSearchResults(search_id, 0, result_count, &node_ids);
  ASSERT_TRUE(node_ids);
  EXPECT_EQ(node_ids->size(), 1u);
  node_ids.reset();

  // Won't match substring of ID for View element.
  dom_agent()->performSearch("id: kTestElement", false, &search_id,
                             &result_count);
  EXPECT_EQ(result_count, 0);
  dom_agent()->getSearchResults(search_id, 0, 1, &node_ids);
  EXPECT_TRUE(!node_ids);

  // Won't match empty query.
  dom_agent()->performSearch("id:", false, &search_id, &result_count);
  EXPECT_EQ(result_count, 0);
  dom_agent()->getSearchResults(search_id, 0, 1, &node_ids);
  EXPECT_TRUE(!node_ids);
}

}  // namespace ui_devtools
