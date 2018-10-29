// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/view_element.h"

#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "ui/views/test/views_test_base.h"

namespace ui_devtools {

using ::testing::_;

class NamedTestView : public views::View {
 public:
  static const char kViewClassName[];
  const char* GetClassName() const override { return kViewClassName; }

  // For custom properties test.
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override {
    *tooltip = base::ASCIIToUTF16("This is the tooltip");
    return true;
  }
};
const char NamedTestView::kViewClassName[] = "NamedTestView";

class ViewElementTest : public views::ViewsTestBase {
 public:
  ViewElementTest() {}
  ~ViewElementTest() override {}

 protected:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    view_.reset(new NamedTestView);
    delegate_.reset(new testing::NiceMock<MockUIElementDelegate>);
    // |OnUIElementAdded| is called on element creation.
    EXPECT_CALL(*delegate_, OnUIElementAdded(_, _)).Times(1);
    element_.reset(new ViewElement(view_.get(), delegate_.get(), nullptr));
  }

  NamedTestView* view() { return view_.get(); }
  ViewElement* element() { return element_.get(); }
  MockUIElementDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<NamedTestView> view_;
  std::unique_ptr<ViewElement> element_;
  std::unique_ptr<MockUIElementDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ViewElementTest);
};

TEST_F(ViewElementTest, SettingsBoundsOnViewCallsDelegate) {
  EXPECT_CALL(*delegate(), OnUIElementBoundsChanged(element())).Times(1);
  view()->SetBounds(1, 2, 3, 4);
}

TEST_F(ViewElementTest, AddingChildView) {
  // The first call is from the element being created, before it
  // gets parented to |element_|.
  EXPECT_CALL(*delegate(), OnUIElementAdded(nullptr, _)).Times(1);
  EXPECT_CALL(*delegate(), OnUIElementAdded(element(), _)).Times(1);
  views::View child_view;
  view()->AddChildView(&child_view);

  DCHECK_EQ(element()->children().size(), 1U);
  UIElement* child_element = element()->children()[0];

  EXPECT_CALL(*delegate(), OnUIElementRemoved(child_element)).Times(1);
  view()->RemoveChildView(&child_view);
}

TEST_F(ViewElementTest, SettingsBoundsOnElementSetsOnView) {
  DCHECK(view()->bounds() == gfx::Rect());

  element()->SetBounds(gfx::Rect(1, 2, 3, 4));
  EXPECT_EQ(view()->bounds(), gfx::Rect(1, 2, 3, 4));
}

TEST_F(ViewElementTest, SettingVisibleOnElementSetsOnView) {
  DCHECK(view()->visible());

  element()->SetVisible(false);
  EXPECT_FALSE(view()->visible());

  element()->SetVisible(true);
  EXPECT_TRUE(view()->visible());
}

TEST_F(ViewElementTest, GetVisible) {
  bool visible;

  view()->SetVisible(false);
  element()->GetVisible(&visible);
  EXPECT_FALSE(visible);

  view()->SetVisible(true);
  element()->GetVisible(&visible);
  EXPECT_TRUE(visible);
}

TEST_F(ViewElementTest, GetBounds) {
  gfx::Rect bounds;

  view()->SetBounds(10, 20, 30, 40);
  element()->GetBounds(&bounds);
  EXPECT_EQ(bounds, gfx::Rect(10, 20, 30, 40));
}

TEST_F(ViewElementTest, GetAttributes) {
  std::unique_ptr<protocol::Array<std::string>> attrs =
      element()->GetAttributes();
  DCHECK_EQ(attrs->length(), 2U);

  EXPECT_EQ(attrs->get(0), "name");
  EXPECT_EQ(attrs->get(1), NamedTestView::kViewClassName);
}

TEST_F(ViewElementTest, GetCustomProperties) {
  auto props = element()->GetCustomProperties();
  DCHECK_EQ(props.size(), 1U);

  EXPECT_EQ(props[0].first, "tooltip");
  EXPECT_EQ(props[0].second, "This is the tooltip");
}

TEST_F(ViewElementTest, GetNodeWindowAndBounds) {
  // For this to be meaningful, the view must be in
  // a widget.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);
  widget->Show();

  widget->GetContentsView()->AddChildView(view());
  gfx::Rect bounds(50, 60, 70, 80);
  view()->SetBoundsRect(bounds);

  std::pair<gfx::NativeWindow, gfx::Rect> window_and_bounds =
      element()->GetNodeWindowAndBounds();
  EXPECT_EQ(window_and_bounds.first, widget->GetNativeWindow());
  EXPECT_EQ(window_and_bounds.second, view()->GetBoundsInScreen());

  view()->parent()->RemoveChildView(view());
}

}  // namespace ui_devtools
