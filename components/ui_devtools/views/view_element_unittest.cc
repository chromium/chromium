// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/view_element.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/protocol.h"
#include "components/ui_devtools/ui_devtools_unittest_utils.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/test/views_test_base.h"

namespace ui_devtools {

namespace {

// Returns a pair of the ClassProperties index and UIProperty index
std::pair<size_t, size_t> GetPropertyIndices(ui_devtools::ViewElement* element,
                                             const std::string& property_name) {
  std::vector<UIElement::ClassProperties> props =
      element->GetCustomPropertiesForMatchedStyle();
  for (size_t cp_index = 0; cp_index < props.size(); ++cp_index) {
    for (size_t uip_index = 0; uip_index < props[cp_index].properties_.size();
         ++uip_index) {
      if (props[cp_index].properties_[uip_index].name_ == property_name)
        return std::make_pair(cp_index, uip_index);
    }
  }
  DCHECK(false) << "Property " << property_name << " can not be found.";
  return std::make_pair(-1, -1);
}

void TestBooleanCustomPropertySetting(ui_devtools::ViewElement* element,
                                      const std::string& property_name,
                                      bool init_value) {
  std::pair<size_t, size_t> indices =
      GetPropertyIndices(element, property_name);
  std::string old_value(init_value ? "true" : "false");
  std::vector<UIElement::ClassProperties> props =
      element->GetCustomPropertiesForMatchedStyle();
  std::vector<UIElement::UIProperty> ui_props =
      props[indices.first].properties_;
  EXPECT_EQ(ui_props[indices.second].value_, old_value);

  // Check the property can be set accordingly.
  std::string new_value(init_value ? "false" : "true");
  std::string separator(":");
  element->SetPropertiesFromString(property_name + separator + new_value);
  props = element->GetCustomPropertiesForMatchedStyle();
  ui_props = props[indices.first].properties_;
  EXPECT_EQ(ui_props[indices.second].name_, property_name);
  EXPECT_EQ(ui_props[indices.second].value_, new_value);

  element->SetPropertiesFromString(property_name + separator + old_value);
  props = element->GetCustomPropertiesForMatchedStyle();
  ui_props = props[indices.first].properties_;
  EXPECT_EQ(ui_props[indices.second].name_, property_name);
  EXPECT_EQ(ui_props[indices.second].value_, old_value);
}

}  // namespace

using ::testing::_;

class MockNamedTestView : public views::View {
  METADATA_HEADER(MockNamedTestView, views::View)

 public:
  // For custom properties test.
  std::u16string GetTooltipText(const gfx::Point& p) const override {
    return u"This is the tooltip";
  }

  int GetBoolProperty() const { return bool_property_; }
  void SetBoolProperty(bool bool_property) { bool_property_ = bool_property; }

  SkColor GetColorProperty() const { return color_property_; }
  void SetColorProperty(SkColor color) { color_property_ = color; }

  MOCK_METHOD1(OnMousePressed, bool(const ui::MouseEvent&));
  MOCK_METHOD1(OnMouseDragged, bool(const ui::MouseEvent&));
  MOCK_METHOD1(OnMouseReleased, void(const ui::MouseEvent&));
  MOCK_METHOD1(OnMouseMoved, void(const ui::MouseEvent&));
  MOCK_METHOD1(OnMouseEntered, void(const ui::MouseEvent&));
  MOCK_METHOD1(OnMouseExited, void(const ui::MouseEvent&));
  MOCK_METHOD1(OnMouseWheel, bool(const ui::MouseWheelEvent&));

 private:
  bool bool_property_ = false;
  SkColor color_property_ = SK_ColorGRAY;
};

BEGIN_METADATA(MockNamedTestView)
ADD_PROPERTY_METADATA(bool, BoolProperty)
ADD_PROPERTY_METADATA(SkColor, ColorProperty, ui::metadata::SkColorConverter)
END_METADATA

class AlwaysOnTopView : public views::View {
  METADATA_HEADER(AlwaysOnTopView, views::View)
};
BEGIN_METADATA(AlwaysOnTopView)
END_METADATA

class SelfReorderingTestView : public views::View, public views::ViewObserver {
  METADATA_HEADER(SelfReorderingTestView, views::View)

 public:
  SelfReorderingTestView()
      : always_on_top_view_(AddChildView(std::make_unique<AlwaysOnTopView>())) {
    AddObserver(this);
  }
  ~SelfReorderingTestView() override { RemoveObserver(this); }

  // views::ViewObserver
  void OnChildViewAdded(View* observed_view, View* child) override {
    ReorderChildView(always_on_top_view_, children().size());
  }

 private:
  raw_ptr<views::View> always_on_top_view_;
};
BEGIN_METADATA(SelfReorderingTestView)
END_METADATA

class ViewElementTest : public views::ViewsTestBase {
 public:
  ViewElementTest() = default;

  ViewElementTest(const ViewElementTest&) = delete;
  ViewElementTest& operator=(const ViewElementTest&) = delete;

  ~ViewElementTest() override = default;

 protected:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    view_ = std::make_unique<testing::NiceMock<MockNamedTestView>>();
    delegate_ = std::make_unique<testing::NiceMock<MockUIElementDelegate>>();
    // |OnUIElementAdded| is called on element creation.
    EXPECT_CALL(*delegate_, OnUIElementAdded(_, _)).Times(1);
    element_ =
        std::make_unique<ViewElement>(view_.get(), delegate_.get(), nullptr);
  }

  MockNamedTestView* view() { return view_.get(); }
  ViewElement* element() { return element_.get(); }
  MockUIElementDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<MockNamedTestView> view_;
  std::unique_ptr<ViewElement> element_;
  std::unique_ptr<MockUIElementDelegate> delegate_;
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

TEST_F(ViewElementTest, SetPropertiesFromString) {
  static const char* kEnabledProperty = "Enabled";
  TestBooleanCustomPropertySetting(element(), kEnabledProperty, true);
  std::pair<size_t, size_t> indices =
      GetPropertyIndices(element(), kEnabledProperty);

  // Test setting a non-existent property has no effect.
  element()->SetPropertiesFromString("Enable:false");
  std::vector<UIElement::ClassProperties> props =
      element()->GetCustomPropertiesForMatchedStyle();
  std::vector<UIElement::UIProperty> ui_props =
      props[indices.first].properties_;

  EXPECT_EQ(ui_props[indices.second].name_, kEnabledProperty);
  EXPECT_EQ(ui_props[indices.second].value_, "true");

  // Test setting empty string for property value has no effect.
  element()->SetPropertiesFromString("Enabled:");
  props = element()->GetCustomPropertiesForMatchedStyle();
  ui_props = props[indices.first].properties_;
  EXPECT_EQ(ui_props[indices.second].name_, kEnabledProperty);
  EXPECT_EQ(ui_props[indices.second].value_, "true");

  // Ensure setting pure whitespace doesn't crash.
  ASSERT_NO_FATAL_FAILURE(element()->SetPropertiesFromString("   \n  "));
}

TEST_F(ViewElementTest, SettingVisibilityOnView) {
  TestBooleanCustomPropertySetting(element(), "Visible", true);
}

TEST_F(ViewElementTest, SettingSubclassBoolProperty) {
  TestBooleanCustomPropertySetting(element(), "BoolProperty", false);
}

TEST_F(ViewElementTest, GetBounds) {
  gfx::Rect bounds;

  view()->SetBounds(10, 20, 30, 40);
  element()->GetBounds(&bounds);
  EXPECT_EQ(bounds, gfx::Rect(10, 20, 30, 40));
}

TEST_F(ViewElementTest, GetAttributes) {
  std::vector<std::string> attrs = element()->GetAttributes();
  EXPECT_THAT(attrs, testing::ElementsAre("class", "MockNamedTestView", "name",
                                          "MockNamedTestView"));
}

TEST_F(ViewElementTest, GetCustomProperties) {
  std::vector<UIElement::ClassProperties> props =
      element()->GetCustomPropertiesForMatchedStyle();

  // The ClassProperties vector should be of size 2.
  DCHECK_EQ(props.size(), 2U);

  std::pair<size_t, size_t> indices =
      GetPropertyIndices(element(), "BoolProperty");
  // The BoolProperty property should be in the second ClassProperties object in
  // the vector.
  DCHECK_EQ(indices.first, 0U);

  indices = GetPropertyIndices(element(), "Tooltip");
  // The tooltip property should be in the second ClassProperties object in the
  // vector.
  DCHECK_EQ(indices.first, 1U);

  std::vector<UIElement::UIProperty> ui_props =
      props[indices.first].properties_;

  EXPECT_EQ(ui_props[indices.second].name_, "Tooltip");
  EXPECT_EQ(ui_props[indices.second].value_, "This is the tooltip");
}

TEST_F(ViewElementTest, CheckCustomProperties) {
  std::vector<UIElement::ClassProperties> props =
      element()->GetCustomPropertiesForMatchedStyle();
  DCHECK_GT(props.size(), 1U);
  DCHECK_GT(props[1].properties_.size(), 1U);

  // Check visibility information is passed in.
  bool is_visible_set = false;
  for (size_t i = 0; i < props[1].properties_.size(); ++i) {
    if (props[1].properties_[i].name_ == "Visible")
      is_visible_set = true;
  }
  EXPECT_TRUE(is_visible_set);
}

TEST_F(ViewElementTest, GetNodeWindowAndScreenBounds) {
  // For this to be meaningful, the view must be in
  // a widget.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(params));
  widget->Show();

  widget->GetContentsView()->AddChildView(view());
  gfx::Rect bounds(50, 60, 70, 80);
  view()->SetBoundsRect(bounds);

  std::pair<gfx::NativeWindow, gfx::Rect> window_and_bounds =
      element()->GetNodeWindowAndScreenBounds();
  EXPECT_EQ(window_and_bounds.first, widget->GetNativeWindow());
  EXPECT_EQ(window_and_bounds.second, view()->GetBoundsInScreen());

  view()->parent()->RemoveChildView(view());
}

TEST_F(ViewElementTest, ColorProperty) {
  EXPECT_EQ(GetPropertyIndices(element(), "--ColorProperty").first, 0U);
  DCHECK_EQ(view()->GetColorProperty(), SK_ColorGRAY);

  EXPECT_TRUE(element()->SetPropertiesFromString(
      "--ColorProperty: rgba(0,0,  255, 1);"));
  EXPECT_EQ(view()->GetColorProperty(), SK_ColorBLUE);

  EXPECT_TRUE(element()->SetPropertiesFromString(
      "--ColorProperty: hsl(240, 84%, 28%);"));
  EXPECT_EQ(view()->GetColorProperty(), SkColorSetARGB(255, 0x0B, 0x0B, 0x47));

  EXPECT_TRUE(element()->SetPropertiesFromString(
      "--ColorProperty: hsla(240, 84%, 28%, 0.5);"));
  EXPECT_EQ(view()->GetColorProperty(), SkColorSetARGB(128, 0x0B, 0x0B, 0x47));
}

TEST_F(ViewElementTest, BadColorProperty) {
  DCHECK_EQ(view()->GetColorProperty(), SK_ColorGRAY);

  element()->SetPropertiesFromString("--ColorProperty: #0352fc");
  EXPECT_EQ(view()->GetColorProperty(), SK_ColorGRAY);

  element()->SetPropertiesFromString("--ColorProperty: rgba(1,2,3,4);");
  EXPECT_EQ(view()->GetColorProperty(), SK_ColorGRAY);

  element()->SetPropertiesFromString("--ColorProperty: rgba(1,2,3,4;");
  EXPECT_EQ(view()->GetColorProperty(), SK_ColorGRAY);

  element()->SetPropertiesFromString("--ColorProperty: rgb(1,2,3,4;)");
  EXPECT_EQ(view()->GetColorProperty(), SK_ColorGRAY);
}

TEST_F(ViewElementTest, GetSources) {
  std::vector<UIElement::Source> sources = element()->GetSources();

  // ViewElement should have two sources: from MockNamedTestView and from View.
  EXPECT_EQ(sources.size(), 2U);
#if defined(__clang__) && defined(_MSC_VER)
  EXPECT_EQ(sources[0].path_,
            "components\\ui_devtools\\views\\view_element_unittest.cc");
  EXPECT_EQ(sources[1].path_, "ui\\views\\view.h");
#else
  EXPECT_EQ(sources[0].path_,
            "components/ui_devtools/views/view_element_unittest.cc");
  EXPECT_EQ(sources[1].path_, "ui/views/view.h");
#endif
}

TEST_F(ViewElementTest, DispatchMouseEvent) {
  // The view must be in a widget in order to dispatch mouse event correctly.
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params =
      CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(params));
  widget->GetContentsView()->AddChildView(view());
  widget->Show();
  gfx::Rect bounds(50, 60, 70, 80);
  view()->SetBoundsRect(bounds);
  std::pair<gfx::NativeWindow, gfx::Rect> window_and_bounds =
      element()->GetNodeWindowAndScreenBounds();
  EXPECT_EQ(window_and_bounds.first, widget->GetNativeWindow());
  EXPECT_EQ(window_and_bounds.second, view()->GetBoundsInScreen());

  EXPECT_CALL(*view(), OnMousePressed(_)).Times(1);
  EXPECT_CALL(*view(), OnMouseDragged(_)).Times(1);
  EXPECT_CALL(*view(), OnMouseReleased(_)).Times(1);
  EXPECT_CALL(*view(), OnMouseMoved(_)).Times(1);
  EXPECT_CALL(*view(), OnMouseEntered(_)).Times(1);
  EXPECT_CALL(*view(), OnMouseExited(_)).Times(1);
  EXPECT_CALL(*view(), OnMouseWheel(_)).Times(1);
  std::vector<std::unique_ptr<protocol::DOM::MouseEvent>> events;
  events.emplace_back(
      protocol::DOM::MouseEvent::create()
          .setType(protocol::DOM::MouseEvent::TypeEnum::MousePressed)
          .setX(0)
          .setY(0)
          .setButton(protocol::DOM::MouseEvent::ButtonEnum::Left)
          .setWheelDirection(
              protocol::DOM::MouseEvent::WheelDirectionEnum::None)
          .build());
  events.emplace_back(
      protocol::DOM::MouseEvent::create()
          .setType(protocol::DOM::MouseEvent::TypeEnum::MouseDragged)
          .setX(0)
          .setY(0)
          .setButton(protocol::DOM::MouseEvent::ButtonEnum::Left)
          .setWheelDirection(
              protocol::DOM::MouseEvent::WheelDirectionEnum::None)
          .build());
  events.emplace_back(
      protocol::DOM::MouseEvent::create()
          .setType(protocol::DOM::MouseEvent::TypeEnum::MouseReleased)
          .setX(0)
          .setY(0)
          .setButton(protocol::DOM::MouseEvent::ButtonEnum::Left)
          .setWheelDirection(
              protocol::DOM::MouseEvent::WheelDirectionEnum::None)
          .build());
  events.emplace_back(
      protocol::DOM::MouseEvent::create()
          .setType(protocol::DOM::MouseEvent::TypeEnum::MouseMoved)
          .setX(0)
          .setY(0)
          .setButton(protocol::DOM::MouseEvent::ButtonEnum::None)
          .setWheelDirection(
              protocol::DOM::MouseEvent::WheelDirectionEnum::None)
          .build());
  events.emplace_back(
      protocol::DOM::MouseEvent::create()
          .setType(protocol::DOM::MouseEvent::TypeEnum::MouseEntered)
          .setX(0)
          .setY(0)
          .setButton(protocol::DOM::MouseEvent::ButtonEnum::None)
          .setWheelDirection(
              protocol::DOM::MouseEvent::WheelDirectionEnum::None)
          .build());
  events.emplace_back(
      protocol::DOM::MouseEvent::create()
          .setType(protocol::DOM::MouseEvent::TypeEnum::MouseExited)
          .setX(0)
          .setY(0)
          .setButton(protocol::DOM::MouseEvent::ButtonEnum::None)
          .setWheelDirection(
              protocol::DOM::MouseEvent::WheelDirectionEnum::None)
          .build());
  events.emplace_back(
      protocol::DOM::MouseEvent::create()
          .setType(protocol::DOM::MouseEvent::TypeEnum::MouseWheel)
          .setX(0)
          .setY(0)
          .setButton(protocol::DOM::MouseEvent::ButtonEnum::None)
          .setWheelDirection(protocol::DOM::MouseEvent::WheelDirectionEnum::Up)
          .build());
  for (auto& event : events)
    element()->DispatchMouseEvent(event.get());

  view()->parent()->RemoveChildView(view());
}

TEST_F(ViewElementTest, OutOfOrderObserverTest) {
  // Override the expectation from setup; we don't need to keep track
  // in this test.
  EXPECT_CALL(*delegate(), OnUIElementAdded(_, _)).Times(testing::AnyNumber());
  auto view = std::make_unique<SelfReorderingTestView>();
  auto element = std::make_unique<ViewElement>(view.get(), delegate(), nullptr);
  // `element` will receive OnChildViewReordered and OnViewAdded out of
  // order. Ensure it doesn't crash and that the subtree is consistent
  // afterward.
  view->AddChildView(std::make_unique<views::View>());
  ASSERT_EQ(element->children().size(), 2u);
  auto attrs = element->children().at(1)->GetAttributes();
  EXPECT_EQ(attrs[0], "class");
  std::string& name = attrs[1];
  EXPECT_EQ(name, "AlwaysOnTopView");
}

}  // namespace ui_devtools
