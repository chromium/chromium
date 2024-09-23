// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/widget_element.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/ui_devtools/protocol.h"
#include "components/ui_devtools/root_element.h"
#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "components/ui_devtools/views/view_element.h"
#include "ui/views/test/views_test_base.h"

namespace ui_devtools {

using ::testing::_;

namespace {
const std::string kWidgetName = "A test widget";
}

class MockWidgetElementDelegate : public MockUIElementDelegate {
 public:
  MockWidgetElementDelegate() = default;
  MockWidgetElementDelegate(const MockWidgetElementDelegate&) = delete;
  MockWidgetElementDelegate& operator=(const MockWidgetElementDelegate&) =
      delete;
  ~MockWidgetElementDelegate() override = default;

  UIElement* root_element() { return &root_element_; }

 private:
  RootElement root_element_{this};
};

class WidgetElementTest : public views::ViewsTestBase {
 public:
  WidgetElementTest() = default;
  WidgetElementTest(const WidgetElementTest&) = delete;
  WidgetElementTest& operator=(const WidgetElementTest&) = delete;
  ~WidgetElementTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    widget_ = new views::Widget;
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW);
    params.name = kWidgetName;
    widget_->Init(std::move(params));

    delegate_ =
        std::make_unique<testing::NiceMock<MockWidgetElementDelegate>>();
    element_ =
        std::make_unique<WidgetElement>(widget_, delegate_.get(), nullptr);
    // The widget element will delete the ViewElement in |OnWillRemoveView|
    // TODO(lgrey): I think probably WidgetElement should do this itself
    // rather than making the DOMAgent (or test)  put the tree together.
    element()->AddChild(
        new ViewElement(widget_->GetRootView(), delegate_.get(), element()));
  }

  void TearDown() override {
    if (widget())
      CloseWidget();
    views::ViewsTestBase::TearDown();
  }

 protected:
  views::Widget* widget() { return widget_; }
  void CloseWidget() {
    widget_->CloseNow();
    widget_ = nullptr;
  }
  WidgetElement* element() { return element_.get(); }
  MockWidgetElementDelegate* delegate() { return delegate_.get(); }
  void AddWidgetElementToTree() {
    element_->set_parent(delegate_->root_element());
    delegate_->root_element()->AddChild(element_.get());
  }

 private:
  raw_ptr<views::Widget, DanglingUntriaged> widget_ = nullptr;
  std::unique_ptr<WidgetElement> element_;
  std::unique_ptr<MockWidgetElementDelegate> delegate_;
};

TEST_F(WidgetElementTest, SettingsBoundsOnWidgetCallsDelegate) {
  // Once for the root view, and once for the widget.
  EXPECT_CALL(*delegate(), OnUIElementBoundsChanged(_))
      .Times(::testing::AtLeast(1));
  EXPECT_CALL(*delegate(), OnUIElementBoundsChanged(element()))
      .Times(::testing::AtLeast(1));
  widget()->SetBounds(gfx::Rect(10, 20, 300, 400));
}

TEST_F(WidgetElementTest, SettingsBoundsOnElementSetsOnWidget) {
  gfx::Rect old_bounds = widget()->GetRestoredBounds();
  gfx::Rect new_bounds = gfx::Rect(10, 20, 300, 400);
  DCHECK(old_bounds != new_bounds);

  element()->SetBounds(new_bounds);
  EXPECT_EQ(widget()->GetRestoredBounds(), new_bounds);
}

TEST_F(WidgetElementTest, SettingVisibleOnElementSetsOnWidget) {
  DCHECK(!widget()->IsVisible());

  element()->SetVisible(true);
  EXPECT_TRUE(widget()->IsVisible());

  element()->SetVisible(false);
  EXPECT_FALSE(widget()->IsVisible());
}

TEST_F(WidgetElementTest, GetBounds) {
  gfx::Rect actual_bounds;

  gfx::Rect new_bounds = gfx::Rect(10, 20, 300, 400);
  widget()->SetBounds(new_bounds);
  element()->GetBounds(&actual_bounds);
  EXPECT_EQ(actual_bounds, new_bounds);
}

TEST_F(WidgetElementTest, GetVisible) {
  DCHECK(!widget()->IsVisible());
  bool visible;

  element()->GetVisible(&visible);
  EXPECT_FALSE(visible);

  widget()->Show();
  element()->GetVisible(&visible);
  EXPECT_TRUE(visible);

  widget()->Hide();
  element()->GetVisible(&visible);
  EXPECT_FALSE(visible);
}

TEST_F(WidgetElementTest, GetAttributes) {
  std::vector<std::string> attrs = element()->GetAttributes();

  ASSERT_FALSE(widget()->IsActive());
  EXPECT_THAT(attrs,
              testing::ElementsAre("name", kWidgetName, "active", "false"));

  widget()->Activate();
  attrs = element()->GetAttributes();
  EXPECT_THAT(attrs,
              testing::ElementsAre("name", kWidgetName, "active", "true"));
}

TEST_F(WidgetElementTest, GetNodeWindowAndScreenBounds) {
  std::pair<gfx::NativeWindow, gfx::Rect> window_and_bounds =
      element()->GetNodeWindowAndScreenBounds();
  EXPECT_EQ(widget()->GetNativeWindow(), window_and_bounds.first);
  EXPECT_EQ(widget()->GetWindowBoundsInScreen(), window_and_bounds.second);
}

TEST_F(WidgetElementTest, TrackNonParentedElementLifetime) {
  ASSERT_TRUE(widget());
  EXPECT_FALSE(element()->parent());
  // While closing the widget, the widget element and its children should be
  // removed as well.
  // Remove the root view element.
  EXPECT_CALL(*delegate(), OnUIElementRemoved(_));
  // Remove the widget element.
  EXPECT_CALL(*delegate(), OnUIElementRemoved(element()));
  CloseWidget();
}

TEST_F(WidgetElementTest, TrackParentedElementLifetime) {
  ASSERT_TRUE(widget());
  AddWidgetElementToTree();
  UIElement* root = delegate()->root_element();
  EXPECT_EQ(root, element()->parent());
  EXPECT_EQ(1u, root->children().size());

  // Remove the root view element.
  EXPECT_CALL(*delegate(), OnUIElementRemoved(_));
  // Remove the widget element.
  EXPECT_CALL(*delegate(), OnUIElementRemoved(element()));
  CloseWidget();
  // After closing the widget, the element tree should only has the root.
  EXPECT_EQ(0u, root->children().size());
}

}  // namespace ui_devtools
