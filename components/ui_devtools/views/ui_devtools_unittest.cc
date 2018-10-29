// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/css_agent.h"
#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/views/dom_agent_aura.h"
#include "components/ui_devtools/views/overlay_agent_aura.h"
#include "components/ui_devtools/views/view_element.h"
#include "components/ui_devtools/views/widget_element.h"
#include "components/ui_devtools/views/window_element.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ui_devtools {
namespace {

using namespace ui_devtools::protocol;

const int kDefaultChildNodeCount = -1;

class TestView : public views::View {
 public:
  TestView(const char* name) : name_(name) {}

  const char* GetClassName() const override { return name_; }

 private:
  const char* name_;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

std::string GetAttributeValue(const std::string& attribute, DOM::Node* node) {
  EXPECT_TRUE(node->hasAttributes());
  Array<std::string>* attributes = node->getAttributes(nullptr);
  for (size_t i = 0; i < attributes->length() - 1; i++) {
    if (attributes->get(i) == attribute)
      return attributes->get(i + 1);
  }
  return nullptr;
}

bool Equals(aura::Window* window, DOM::Node* node) {
  int children_count = static_cast<int>(window->children().size());
  if (views::Widget::GetWidgetForNativeView(window))
    children_count++;
  return "Window" == node->getNodeName() &&
         window->GetName() == GetAttributeValue("name", node) &&
         children_count == node->getChildNodeCount(kDefaultChildNodeCount);
}

void Compare(views::Widget* widget, DOM::Node* node) {
  EXPECT_EQ("Widget", node->getNodeName());
  EXPECT_EQ(widget->GetName(), GetAttributeValue("name", node));
  EXPECT_EQ(widget->GetRootView() ? 1 : 0,
            node->getChildNodeCount(kDefaultChildNodeCount));
}

void Compare(views::View* view, DOM::Node* node) {
  EXPECT_EQ("View", node->getNodeName());
  EXPECT_EQ(view->GetClassName(), GetAttributeValue("name", node));
  EXPECT_EQ(view->child_count(),
            node->getChildNodeCount(kDefaultChildNodeCount));
}

void Compare(aura::Window* window, DOM::Node* node) {
  EXPECT_TRUE(Equals(window, node));
}

DOM::Node* FindInRoot(aura::Window* window, DOM::Node* root) {
  if (Equals(window, root))
    return root;

  Array<DOM::Node>* children = root->getChildren(nullptr);
  DOM::Node* window_node = nullptr;
  for (size_t i = 0; i < children->length(); i++) {
    window_node = FindInRoot(window, children->get(i));
    if (window_node)
      return window_node;
  }
  return window_node;
}

}  // namespace

class UIDevToolsTest : public views::ViewsTestBase {
 public:
  UIDevToolsTest() {}
  ~UIDevToolsTest() override {}

  views::internal::NativeWidgetPrivate* CreateTestNativeWidget() {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params;
    params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
    params.parent = GetPrimaryRootWindow();
    widget->Init(params);
    return widget->native_widget_private();
  }

  std::unique_ptr<views::Widget> CreateTestWidget(const gfx::Rect& bounds) {
    std::unique_ptr<views::Widget> widget(new views::Widget);
    views::Widget::InitParams params;
    params.delegate = nullptr;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = bounds;
    params.parent = GetPrimaryRootWindow();
    widget->Init(params);
    widget->Show();
    return widget;
  }

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

  void SetUp() override {
    fake_frontend_channel_ = std::make_unique<FakeFrontendChannel>();
    uber_dispatcher_ =
        std::make_unique<UberDispatcher>(fake_frontend_channel_.get());
    dom_agent_ = std::make_unique<DOMAgentAura>();
    dom_agent_->Init(uber_dispatcher_.get());
    css_agent_ = std::make_unique<CSSAgent>(dom_agent_.get());
    css_agent_->Init(uber_dispatcher_.get());
    css_agent_->enable();
    overlay_agent_ = std::make_unique<OverlayAgentAura>(dom_agent_.get());
    overlay_agent_->Init(uber_dispatcher_.get());
    overlay_agent_->enable();

    // We need to create |dom_agent| first to observe creation of
    // WindowTreeHosts in ViewTestBase::SetUp().
    views::ViewsTestBase::SetUp();

    top_window = CreateChildWindow(GetPrimaryRootWindow());
    top_default_container_window = CreateChildWindow(GetPrimaryRootWindow());
    top_overlay_window = CreateChildWindow(GetPrimaryRootWindow(),
                                           aura::client::WINDOW_TYPE_UNKNOWN);
  }

  void TearDown() override {
    top_overlay_window.reset();
    top_default_container_window.reset();
    top_window.reset();
    css_agent_.reset();
    overlay_agent_.reset();
    dom_agent_.reset();
    uber_dispatcher_.reset();
    fake_frontend_channel_.reset();
    views::ViewsTestBase::TearDown();
  }

  void ExpectChildNodeInserted(int parent_id, int prev_sibling_id) {
    EXPECT_EQ(1, frontend_channel()->CountProtocolNotificationMessageStartsWith(
                     base::StringPrintf("{\"method\":\"DOM.childNodeInserted\","
                                        "\"params\":{\"parentNodeId\":%d,"
                                        "\"previousNodeId\":%d",
                                        parent_id, prev_sibling_id)));
  }

  void ExpectChildNodeRemoved(int parent_id, int node_id) {
    EXPECT_EQ(1, frontend_channel()->CountProtocolNotificationMessage(
                     base::StringPrintf(
                         "{\"method\":\"DOM.childNodeRemoved\",\"params\":{"
                         "\"parentNodeId\":%d,\"nodeId\":%d}}",
                         parent_id, node_id)));
  }

  FakeFrontendChannel* frontend_channel() {
    return fake_frontend_channel_.get();
  }

  aura::Window* GetPrimaryRootWindow() {
    DCHECK(dom_agent()->root_windows().size());
    return dom_agent()->root_windows()[0];
  }

  CSSAgent* css_agent() { return css_agent_.get(); }
  DOMAgentAura* dom_agent() { return dom_agent_.get(); }
  OverlayAgentAura* overlay_agent() { return overlay_agent_.get(); }

  std::unique_ptr<aura::Window> top_overlay_window;
  std::unique_ptr<aura::Window> top_window;
  std::unique_ptr<aura::Window> top_default_container_window;

 private:
  std::unique_ptr<UberDispatcher> uber_dispatcher_;
  std::unique_ptr<FakeFrontendChannel> fake_frontend_channel_;
  std::unique_ptr<DOMAgentAura> dom_agent_;
  std::unique_ptr<CSSAgent> css_agent_;
  std::unique_ptr<OverlayAgentAura> overlay_agent_;

  DISALLOW_COPY_AND_ASSIGN(UIDevToolsTest);
};


TEST_F(UIDevToolsTest, GetDocumentWithWindowWidgetView) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(gfx::Rect(1, 1, 1, 1)));
  aura::Window* parent_window = widget->GetNativeWindow();
  parent_window->SetName("parent_window");
  std::unique_ptr<aura::Window> child_window = CreateChildWindow(parent_window);
  child_window->SetName("child_window");
  widget->Show();
  views::View* child_view = new TestView("child_view");
  widget->GetRootView()->AddChildView(child_view);

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* parent_node = FindInRoot(parent_window, root.get());
  ASSERT_TRUE(parent_node);
  Array<DOM::Node>* parent_children = parent_node->getChildren(nullptr);
  ASSERT_TRUE(parent_children);
  DOM::Node* widget_node = parent_children->get(0);
  Compare(widget.get(), widget_node);
  Compare(child_window.get(), parent_children->get(1));
  Array<DOM::Node>* widget_children = widget_node->getChildren(nullptr);
  ASSERT_TRUE(widget_children);
  Compare(widget->GetRootView(), widget_children->get(0));
  ASSERT_TRUE(widget_children->get(0)->getChildren(nullptr));
  Compare(child_view, widget_children->get(0)->getChildren(nullptr)->get(1));
}

TEST_F(UIDevToolsTest, GetDocumentNativeWidgetOwnsWidget) {
  views::internal::NativeWidgetPrivate* native_widget_private =
      CreateTestNativeWidget();
  views::Widget* widget = native_widget_private->GetWidget();
  aura::Window* parent_window = widget->GetNativeWindow();

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* parent_node = FindInRoot(parent_window, root.get());
  ASSERT_TRUE(parent_node);
  DOM::Node* widget_node = parent_node->getChildren(nullptr)->get(0);
  Compare(widget, widget_node);
  // Destroy NativeWidget followed by |widget|
  widget->CloseNow();
}

TEST_F(UIDevToolsTest, WindowAddedChildNodeInserted) {
  // Initialize DOMAgent
  std::unique_ptr<aura::Window> window_child =
      CreateChildWindow(top_window.get());

  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  aura::Window* root_window = GetPrimaryRootWindow();
  aura::Window* parent_window = root_window->children()[0];
  DOM::Node* parent_node = FindInRoot(parent_window, root.get());
  Array<DOM::Node>* parent_node_children = parent_node->getChildren(nullptr);
  DOM::Node* sibling_node =
      parent_node_children->get(parent_node_children->length() - 1);

  std::unique_ptr<aura::Window> child(CreateChildWindow(parent_window));
  ExpectChildNodeInserted(parent_node->getNodeId(), sibling_node->getNodeId());
}

TEST_F(UIDevToolsTest, WindowDestroyedChildNodeRemoved) {
  std::unique_ptr<aura::Window> child_1 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_2 = CreateChildWindow(child_1.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  aura::Window* root_window = GetPrimaryRootWindow();
  aura::Window* rotation_window = root_window->children()[0];
  aura::Window* parent_window = rotation_window->children()[0];
  aura::Window* child_window = parent_window->children()[0];
  DOM::Node* root_node =
      root->getChildren(nullptr)->get(0)->getChildren(nullptr)->get(0);
  DOM::Node* parent_node = root_node->getChildren(nullptr)->get(0);
  DOM::Node* child_node = parent_node->getChildren(nullptr)->get(0);

  Compare(parent_window, parent_node);
  Compare(child_window, child_node);
  child_2.reset();
  ExpectChildNodeRemoved(parent_node->getNodeId(), child_node->getNodeId());
}

TEST_F(UIDevToolsTest, WindowReorganizedChildNodeRearranged) {
  std::unique_ptr<aura::Window> child_1 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_2 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_11 = CreateChildWindow(child_1.get());
  std::unique_ptr<aura::Window> child_21 = CreateChildWindow(child_2.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  aura::Window* root_window = GetPrimaryRootWindow();
  aura::Window* rotation_window = root_window->children()[0];
  aura::Window* parent_window = rotation_window->children()[0];
  aura::Window* target_window = rotation_window->children()[1];
  aura::Window* child_window = parent_window->children()[0];

  DOM::Node* root_node =
      root->getChildren(nullptr)->get(0)->getChildren(nullptr)->get(0);
  DOM::Node* parent_node = root_node->getChildren(nullptr)->get(0);
  DOM::Node* target_node = root_node->getChildren(nullptr)->get(1);
  Array<DOM::Node>* target_node_children = target_node->getChildren(nullptr);
  DOM::Node* sibling_node =
      target_node_children->get(target_node_children->length() - 1);
  DOM::Node* child_node = parent_node->getChildren(nullptr)->get(0);

  Compare(parent_window, parent_node);
  Compare(target_window, target_node);
  Compare(child_window, child_node);
  target_window->AddChild(child_window);
  ExpectChildNodeRemoved(parent_node->getNodeId(), child_node->getNodeId());
  ExpectChildNodeInserted(target_node->getNodeId(), sibling_node->getNodeId());
}

TEST_F(UIDevToolsTest, WindowReorganizedChildNodeRemovedAndInserted) {
  std::unique_ptr<aura::Window> child_1 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_2 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_21 = CreateChildWindow(child_2.get());
  std::unique_ptr<aura::Window> child_22 = CreateChildWindow(child_2.get());

  aura::Window* root_window = GetPrimaryRootWindow();
  aura::Window* rotation_window = root_window->children()[0];
  aura::Window* parent_window = rotation_window->children()[0];
  aura::Window* target_window = rotation_window->children()[1];
  std::unique_ptr<aura::Window> child_window(CreateChildWindow(parent_window));

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);
  DOM::Node* root_node =
      root->getChildren(nullptr)->get(0)->getChildren(nullptr)->get(0);
  DOM::Node* parent_node = root_node->getChildren(nullptr)->get(0);
  DOM::Node* target_node = root_node->getChildren(nullptr)->get(1);
  Array<DOM::Node>* target_node_children = target_node->getChildren(nullptr);
  DOM::Node* sibling_node =
      target_node_children->get(target_node_children->length() - 1);
  Array<DOM::Node>* parent_node_children = parent_node->getChildren(nullptr);
  DOM::Node* child_node =
      parent_node_children->get(parent_node_children->length() - 1);

  Compare(parent_window, parent_node);
  Compare(target_window, target_node);
  Compare(child_window.get(), child_node);
  parent_window->RemoveChild(child_window.get());
  target_window->AddChild(child_window.get());
  ExpectChildNodeRemoved(parent_node->getNodeId(), child_node->getNodeId());
  ExpectChildNodeInserted(target_node->getNodeId(), sibling_node->getNodeId());
}

TEST_F(UIDevToolsTest, WindowStackingChangedChildNodeRemovedAndInserted) {
  std::unique_ptr<aura::Window> child_11 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_12 = CreateChildWindow(top_window.get());
  std::unique_ptr<aura::Window> child_13 = CreateChildWindow(top_window.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  aura::Window* root_window = GetPrimaryRootWindow();
  aura::Window* parent_window = root_window->children()[0];
  aura::Window* child_window = parent_window->children()[0];
  aura::Window* target_window = parent_window->children()[1];

  DOM::Node* parent_node =
      root->getChildren(nullptr)->get(0)->getChildren(nullptr)->get(0);
  Array<DOM::Node>* parent_node_children = parent_node->getChildren(nullptr);
  DOM::Node* child_node = parent_node_children->get(0);
  DOM::Node* sibling_node = parent_node_children->get(1);
  int parent_id = parent_node->getNodeId();

  Compare(parent_window, parent_node);
  Compare(child_window, child_node);
  parent_window->StackChildAbove(child_window, target_window);
  ExpectChildNodeRemoved(parent_id, child_node->getNodeId());
  ExpectChildNodeInserted(parent_id, sibling_node->getNodeId());
}

TEST_F(UIDevToolsTest, ViewInserted) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(gfx::Rect(1, 1, 1, 1)));
  aura::Window* window = widget->GetNativeWindow();
  widget->Show();

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* parent_node = FindInRoot(window, root.get());
  ASSERT_TRUE(parent_node);
  DOM::Node* widget_node = parent_node->getChildren(nullptr)->get(0);
  DOM::Node* root_view_node = widget_node->getChildren(nullptr)->get(0);
  Array<DOM::Node>* root_view_children = root_view_node->getChildren(nullptr);
  ASSERT_TRUE(root_view_children);
  DOM::Node* sibling_view_node =
      root_view_children->get(root_view_children->length() - 1);

  widget->GetRootView()->AddChildView(new views::View);
  ExpectChildNodeInserted(root_view_node->getNodeId(),
                          sibling_view_node->getNodeId());
}

TEST_F(UIDevToolsTest, ViewRemoved) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(gfx::Rect(1, 1, 1, 1)));
  // Need to store |view| in unique_ptr because it is removed from the widget
  // and needs to be destroyed independently
  std::unique_ptr<views::View> child_view = std::make_unique<views::View>();
  aura::Window* window = widget->GetNativeWindow();
  widget->Show();
  views::View* root_view = widget->GetRootView();
  root_view->AddChildView(child_view.get());

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* parent_node = FindInRoot(window, root.get());
  ASSERT_TRUE(parent_node);
  DOM::Node* widget_node = parent_node->getChildren(nullptr)->get(0);
  DOM::Node* root_view_node = widget_node->getChildren(nullptr)->get(0);
  Array<DOM::Node>* root_view_children = root_view_node->getChildren(nullptr);
  ASSERT_TRUE(root_view_children);
  DOM::Node* child_view_node =
      root_view_children->get(root_view_children->length() - 1);

  Compare(child_view.get(), child_view_node);
  root_view->RemoveChildView(child_view.get());
  ExpectChildNodeRemoved(root_view_node->getNodeId(),
                         child_view_node->getNodeId());
}

TEST_F(UIDevToolsTest, ViewRearranged) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(gfx::Rect(1, 1, 1, 1)));
  aura::Window* window = widget->GetNativeWindow();
  widget->Show();
  views::View* root_view = widget->GetRootView();
  views::View* parent_view = new views::View;
  views::View* target_view = new views::View;
  views::View* child_view = new views::View;
  views::View* child_view_1 = new views::View;

  root_view->AddChildView(parent_view);
  root_view->AddChildView(target_view);
  parent_view->AddChildView(child_view);
  parent_view->AddChildView(child_view_1);

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* parent_node = FindInRoot(window, root.get());
  ASSERT_TRUE(parent_node);
  DOM::Node* widget_node = parent_node->getChildren(nullptr)->get(0);
  DOM::Node* root_view_node = widget_node->getChildren(nullptr)->get(0);
  Array<DOM::Node>* root_view_children = root_view_node->getChildren(nullptr);
  ASSERT_TRUE(root_view_children);
  size_t root_children_size = root_view_children->length();
  ASSERT_TRUE(root_children_size >= 2);
  DOM::Node* parent_view_node = root_view_children->get(root_children_size - 2);
  DOM::Node* target_view_node = root_view_children->get(root_children_size - 1);
  DOM::Node* child_view_node = parent_view_node->getChildren(nullptr)->get(0);
  DOM::Node* child_view_node_1 = parent_view_node->getChildren(nullptr)->get(1);

  Compare(parent_view, parent_view_node);
  Compare(target_view, target_view_node);
  Compare(child_view, child_view_node);
  Compare(child_view_1, child_view_node_1);

  ASSERT_NE(child_view_node->getNodeId(), child_view_node_1->getNodeId());

  // Reorder child_view_1 from index 1 to 0 in view::Views tree. This makes DOM
  // tree remove view node at position 1 and insert it at position 0.
  parent_view->ReorderChildView(child_view_1, 0);
  ExpectChildNodeRemoved(parent_view_node->getNodeId(),
                         child_view_node_1->getNodeId());
  ExpectChildNodeInserted(parent_view_node->getNodeId(), 0);

  // Reorder child_view_1 to the same index 0 shouldn't perform reroder work, so
  // we still expect 1 remove and 1 insert protocol notification messages.
  parent_view->ReorderChildView(child_view_1, 0);
  ExpectChildNodeRemoved(parent_view_node->getNodeId(),
                         child_view_node_1->getNodeId());
  ExpectChildNodeInserted(parent_view_node->getNodeId(), 0);

  target_view->AddChildView(child_view);
  ExpectChildNodeRemoved(parent_view_node->getNodeId(),
                         child_view_node->getNodeId());
  ExpectChildNodeInserted(target_view_node->getNodeId(), 0);
}

TEST_F(UIDevToolsTest, ViewRearrangedRemovedAndInserted) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(gfx::Rect(1, 1, 1, 1)));
  aura::Window* window = widget->GetNativeWindow();
  widget->Show();
  views::View* root_view = widget->GetRootView();
  views::View* parent_view = new views::View;
  views::View* target_view = new views::View;
  views::View* child_view = new views::View;
  root_view->AddChildView(parent_view);
  root_view->AddChildView(target_view);
  parent_view->AddChildView(child_view);

  // Initialize DOMAgent
  std::unique_ptr<DOM::Node> root;
  dom_agent()->getDocument(&root);

  DOM::Node* parent_node = FindInRoot(window, root.get());
  ASSERT_TRUE(parent_node);
  DOM::Node* widget_node = parent_node->getChildren(nullptr)->get(0);
  DOM::Node* root_view_node = widget_node->getChildren(nullptr)->get(0);
  Array<DOM::Node>* root_view_children = root_view_node->getChildren(nullptr);
  ASSERT_TRUE(root_view_children);
  size_t root_children_size = root_view_children->length();
  ASSERT_TRUE(root_children_size >= 2);
  DOM::Node* parent_view_node = root_view_children->get(root_children_size - 2);
  DOM::Node* target_view_node = root_view_children->get(root_children_size - 1);
  DOM::Node* child_view_node = parent_view_node->getChildren(nullptr)->get(0);

  Compare(parent_view, parent_view_node);
  Compare(target_view, target_view_node);
  Compare(child_view, child_view_node);
  parent_view->RemoveChildView(child_view);
  target_view->AddChildView(child_view);
  ExpectChildNodeRemoved(parent_view_node->getNodeId(),
                         child_view_node->getNodeId());
  ExpectChildNodeInserted(target_view_node->getNodeId(), 0);
}


// TODO(thanhph): Make test AshDevToolsTest.MultipleDisplayHighlight work with
// multiple displays. https://crbug.com/726831.

}  // namespace ui_devtools
