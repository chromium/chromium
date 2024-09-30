// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"

#include <gtest/gtest.h>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/overflow_button.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_button_status_indicator.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/actions/actions.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace {
// Toolbar button size is ~34dp.
constexpr gfx::Size kButtonSize(34, 34);

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton3);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton4);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyObservedView);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyActivateView);

class TestDelegate : public ToolbarController::PinnedActionsDelegate {
 public:
  MOCK_METHOD(void,
              DummyAction,
              (actions::ActionItem*, actions::ActionInvocationContext));
  TestDelegate() {
    for (const auto& id : action_ids_) {
      action_items_.push_back(
          actions::ActionItem::ActionItemBuilder(
              base::BindRepeating(&TestDelegate::DummyAction,
                                  base::Unretained(this)))
              .SetActionId(id)
              .SetImage(
                  ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon))
              .SetProperty(kActionItemUnderlineIndicatorKey, true)
              .SetText(
                  base::StrCat({u"DummyAction", base::NumberToString16(id)}))
              .Build());
      if (id == 0) {
        kIdToOverflowedMap_[id] = false;
      } else {
        kIdToOverflowedMap_[id] = true;
        ++overflowed_count_;
      }
      kIdToItemMap_[id] = action_items_[id].get();
    }
  }
  ~TestDelegate() override = default;

  actions::ActionItem* GetActionItemFor(const actions::ActionId& id) override {
    return kIdToItemMap_.at(id);
  }
  bool IsOverflowed(const actions::ActionId& id) override {
    return kIdToOverflowedMap_.at(id);
  }
  views::View* GetContainerView() override { return container_view_; }
  void SetContainerView(views::View* view) { container_view_ = view; }
  bool ShouldAnyButtonsOverflow(gfx::Size available_size) const override {
    return true;
  }
  int get_overflowed_count() { return overflowed_count_; }
  std::vector<actions::ActionId> get_action_ids() { return action_ids_; }

 private:
  int overflowed_count_ = 0;
  std::vector<actions::ActionId> action_ids_ = {0, 1, 2};
  std::vector<std::unique_ptr<actions::ActionItem>> action_items_;
  base::flat_map<actions::ActionId,
                 raw_ptr<actions::ActionItem, CtnExperimental>>
      kIdToItemMap_;
  base::flat_map<actions::ActionId, bool> kIdToOverflowedMap_;
  raw_ptr<views::View> container_view_;
};

class MockToolbarController : public ToolbarController {
 public:
  MockToolbarController(
      const std::vector<ToolbarController::ResponsiveElementInfo>&
          responsive_elements,
      const std::vector<ui::ElementIdentifier>& elements_in_overflow_order,
      int element_flex_order_start,
      views::View* toolbar_container_view,
      OverflowButton* overflow_button,
      TestDelegate* delegate)
      : ToolbarController(responsive_elements,
                          elements_in_overflow_order,
                          element_flex_order_start,
                          toolbar_container_view,
                          overflow_button,
                          delegate) {}
  MOCK_METHOD(bool, PopOut, (ui::ElementIdentifier identifier), (override));
  MOCK_METHOD(bool, EndPopOut, (ui::ElementIdentifier identifier), (override));
};

class PopOutHandlerTest : public ChromeViewsTestBase {
 public:
  PopOutHandlerTest() = default;

  PopOutHandlerTest(const PopOutHandlerTest&) = delete;
  PopOutHandlerTest& operator=(const PopOutHandlerTest&) = delete;

  ~PopOutHandlerTest() override = default;

 protected:
  views::Widget* widget() { return widget_.get(); }
  views::View* container_view() { return container_view_; }
  OverflowButton* overflow_button() { return overflow_button_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View, DanglingUntriaged> container_view_;
  raw_ptr<OverflowButton, DanglingUntriaged> overflow_button_;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();

    std::unique_ptr<views::View> toolbar_container_view =
        std::make_unique<views::View>();
    toolbar_container_view
        ->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetDefault(views::kFlexBehaviorKey,
                    views::FlexSpecification(
                        views::LayoutOrientation::kHorizontal,
                        views::MinimumFlexSizeRule::kPreferredSnapToZero,
                        views::MaximumFlexSizeRule::kPreferred));
    container_view_ =
        widget_->SetContentsView(std::move(toolbar_container_view));

    auto overflow_button = std::make_unique<OverflowButton>();
    overflow_button_ =
        container_view_->AddChildView(std::move(overflow_button));
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }
};

TEST_F(PopOutHandlerTest, PopOutAndEndPopOut) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDummyButton);
  auto test_delegate = std::make_unique<TestDelegate>();

  MockToolbarController toolbar_controller(
      std::vector<ToolbarController::ResponsiveElementInfo>(
          {{ToolbarController::ElementIdInfo{
                kDummyButton, 0, &vector_icons::kErrorIcon, kDummyActivateView},
            false, kDummyObservedView}}),
      std::vector<ui::ElementIdentifier>({kDummyButton}), 1, container_view(),
      overflow_button(), test_delegate.get());

  overflow_button()->set_toolbar_controller(&toolbar_controller);

  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(widget());
  ToolbarController::PopOutHandler pop_out_controller(
      &toolbar_controller, context, kDummyButton, kDummyObservedView);

  EXPECT_CALL(toolbar_controller, PopOut(kDummyButton));
  auto observed_view = std::make_unique<views::View>();
  observed_view->SetProperty(views::kElementIdentifierKey, kDummyObservedView);
  views::View* view = container_view()->AddChildView(std::move(observed_view));

  EXPECT_CALL(toolbar_controller, EndPopOut(kDummyButton));
  container_view()->RemoveChildView(view);
}

constexpr int kElementFlexOrderStart = 1;

}  // namespace

class TestToolbarController : public ToolbarController {
 public:
  TestToolbarController(
      const std::vector<ToolbarController::ResponsiveElementInfo>&
          responsive_elements,
      const std::vector<ui::ElementIdentifier>& elements_in_overflow_order,
      int element_flex_order_start,
      views::View* toolbar_container_view,
      OverflowButton* overflow_button,
      TestDelegate* delegate)
      : ToolbarController(responsive_elements,
                          elements_in_overflow_order,
                          element_flex_order_start,
                          toolbar_container_view,
                          overflow_button,
                          delegate) {}

  std::u16string GetMenuText(const ToolbarController::ResponsiveElementInfo&
                                 element_info) const override {
    static const base::flat_map<ui::ElementIdentifier, std::u16string>
        kToolbarToMenuTextMap = {{kDummyButton1, u"DummyButton1"},
                                 {kDummyButton2, u"DummyButton2"},
                                 {kDummyButton3, u"DummyButton3"},
                                 {kDummyButton4, u"DummyButton4"}};

    return kToolbarToMenuTextMap.at(
        absl::get<ToolbarController::ElementIdInfo>(element_info.overflow_id)
            .overflow_identifier);
  }
};

class ToolbarControllerUnitTest : public ChromeViewsTestBase {
 public:
  ToolbarControllerUnitTest() = default;
  ToolbarControllerUnitTest(const ToolbarControllerUnitTest&) = delete;
  ToolbarControllerUnitTest& operator=(const ToolbarControllerUnitTest&) =
      delete;
  ~ToolbarControllerUnitTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget_->Show();

    std::unique_ptr<views::View> toolbar_container_view =
        std::make_unique<views::View>();
    toolbar_container_view
        ->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetDefault(views::kFlexBehaviorKey,
                    views::FlexSpecification(
                        views::LayoutOrientation::kHorizontal,
                        views::MinimumFlexSizeRule::kPreferredSnapToZero,
                        views::MaximumFlexSizeRule::kPreferred));
    toolbar_container_view_ =
        widget_->SetContentsView(std::move(toolbar_container_view));

    std::vector<ui::ElementIdentifier> element_ids = {
        kDummyButton1, kDummyButton2, kDummyButton3};
    InitToolbarContainerViewWithTestButtons(element_ids);

    auto overflow_button = std::make_unique<OverflowButton>();
    overflow_button_ =
        toolbar_container_view_->AddChildView(std::move(overflow_button));
    overflow_button_->SetVisible(false);
    test_delegate_ = std::make_unique<TestDelegate>();
    toolbar_controller_ = std::make_unique<TestToolbarController>(
        std::vector<ToolbarController::ResponsiveElementInfo>(
            {{ToolbarController::ElementIdInfo{kDummyButton1, 0,
                                               &vector_icons::kErrorIcon,
                                               kDummyActivateView},
              false, kDummyObservedView},
             {ToolbarController::ElementIdInfo{kDummyButton2, 0,
                                               &vector_icons::kErrorIcon,
                                               kDummyActivateView},
              true, kDummyObservedView},
             {ToolbarController::ElementIdInfo{kDummyButton3, 0,
                                               &vector_icons::kErrorIcon,
                                               kDummyActivateView},
              true, kDummyObservedView}}),
        std::vector<ui::ElementIdentifier>(
            {kDummyButton3, kDummyButton2, kDummyButton1}),
        kElementFlexOrderStart, toolbar_container_view_, overflow_button_,
        test_delegate_.get());
    overflow_button_->set_toolbar_controller(toolbar_controller_.get());
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(widget_.get()));
  }

  // Add test buttons with `ids` to `toolbar_container_view_`.
  void InitToolbarContainerViewWithTestButtons(
      std::vector<ui::ElementIdentifier> ids) {
    for (size_t i = 0; i < ids.size(); ++i) {
      auto button = std::make_unique<views::View>();
      button->SetProperty(views::kElementIdentifierKey, ids[i]);
      button->SetPreferredSize(kButtonSize);
      button->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
      button->GetViewAccessibility().SetName(
          base::StrCat({u"DummyButton", base::NumberToString16(i)}));
      button->SetVisible(true);
      test_buttons_.emplace_back(
          toolbar_container_view_->AddChildView(std::move(button)));
    }
  }

  void TearDown() override {
    overflow_button_->set_toolbar_controller(nullptr);
    overflow_button_ = nullptr;
    toolbar_container_view_ = nullptr;
    event_generator_.reset();
    toolbar_controller_.reset();
    widget_.reset();
    test_delegate_.reset();
    ChromeViewsTestBase::TearDown();
  }

  void UpdateOverflowButtonVisibility() {
    toolbar_controller()->overflow_button()->SetVisible(
        toolbar_controller()->ShouldShowOverflowButton(widget()->GetSize()));
  }

  views::Widget* widget() { return widget_.get(); }
  ToolbarController* toolbar_controller() { return toolbar_controller_.get(); }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  views::View* toolbar_container_view() { return toolbar_container_view_; }
  OverflowButton* overflow_button() { return overflow_button_; }
  const std::vector<raw_ptr<views::View, VectorExperimental>>& test_buttons() {
    return test_buttons_;
  }
  const ui::SimpleMenuModel* overflow_menu() {
    return toolbar_controller_->menu_model_for_testing();
  }
  std::vector<const ToolbarController::ResponsiveElementInfo*>
  GetOverflowedElements() {
    return toolbar_controller()->GetOverflowedElements();
  }
  const std::vector<ToolbarController::ResponsiveElementInfo>&
  GetResponsiveElements(const ToolbarController* toolbar_controller) {
    return toolbar_controller->responsive_elements_;
  }
  bool IsOverflowed(const ToolbarController::ResponsiveElementInfo& element) {
    return toolbar_controller_->IsOverflowed(element);
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ToolbarController> toolbar_controller_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  raw_ptr<views::View> toolbar_container_view_;
  raw_ptr<OverflowButton> overflow_button_;
  std::unique_ptr<TestDelegate> test_delegate_;

  // Buttons being tested.
  std::vector<raw_ptr<views::View, VectorExperimental>> test_buttons_;
};

TEST_F(ToolbarControllerUnitTest, OverflowButtonVisibility) {
  // Initialize widget width with total width of all test buttons.
  // Should not see overflowed buttons.
  widget()->SetSize(gfx::Size(kButtonSize.width() * test_buttons().size(),
                              kButtonSize.height()));
  EXPECT_EQ(GetOverflowedElements().size(), size_t(0));
  UpdateOverflowButtonVisibility();
  EXPECT_FALSE(overflow_button()->GetVisible());

  // Shrink widget width with one button width smaller.
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 1),
                              kButtonSize.height()));
  EXPECT_EQ(GetOverflowedElements().size(), size_t(1));
  UpdateOverflowButtonVisibility();
  EXPECT_TRUE(overflow_button()->GetVisible());
}

TEST_F(ToolbarControllerUnitTest, OverflowedButtonsMatchMenu) {
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 1),
                              kButtonSize.height()));
  UpdateOverflowButtonVisibility();
  EXPECT_TRUE(overflow_button()->GetVisible());

  widget()->LayoutRootViewIfNecessary();
  event_generator()->MoveMouseTo(
      overflow_button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();

  const ui::SimpleMenuModel* menu = overflow_menu();
  const auto overflowed_buttons = GetOverflowedElements();

  // Overflowed buttons should match overflow menu.
  EXPECT_TRUE(menu);
  const auto& responsive_elements = GetResponsiveElements(toolbar_controller());
  for (size_t i = 0; i < responsive_elements.size(); ++i) {
    if (IsOverflowed(responsive_elements[i])) {
      EXPECT_EQ(toolbar_controller()->GetMenuText(responsive_elements[i]),
                menu->GetLabelAt(menu->GetIndexOfCommandId(i).value()));
    }
  }
}

TEST_F(ToolbarControllerUnitTest, RunningMenuAddsStatusIndicator) {
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 1),
                              kButtonSize.height()));
  UpdateOverflowButtonVisibility();
  EXPECT_TRUE(overflow_button()->GetVisible());

  widget()->LayoutRootViewIfNecessary();
  event_generator()->MoveMouseTo(
      overflow_button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();

  const ui::SimpleMenuModel* menu = overflow_menu();

  // Overflowed buttons should match overflow menu.
  EXPECT_TRUE(menu);
  views::SubmenuView* sub_menu =
      toolbar_controller()->root_menu_item()->GetSubmenu();

  for (auto* menu_item : sub_menu->GetMenuItems()) {
    PinnedToolbarButtonStatusIndicator* status_indicator = nullptr;

    for (auto& child : menu_item->icon_view()->children()) {
      if (views::AsViewClass<PinnedToolbarButtonStatusIndicator>(child)) {
        status_indicator =
            views::AsViewClass<PinnedToolbarButtonStatusIndicator>(child);
      }

      EXPECT_TRUE(status_indicator);
    }
  }
}

TEST_F(ToolbarControllerUnitTest, MenuSeparator) {
  // Set widget to be small enough to ensure all the buttons overflow.
  widget()->SetSize(gfx::Size(1, 1));
  UpdateOverflowButtonVisibility();

  // All 3 buttons overflowed.
  EXPECT_EQ(GetOverflowedElements().size(), static_cast<size_t>(3));
  const auto menu = toolbar_controller()->CreateOverflowMenuModel();
  EXPECT_TRUE(menu);

  // There is no separator between button1 and 2 because button1 is not a menu
  // section end.
  // There is a separator between button2 and button3 because
  // 1) button2 is a section end;
  // 2) the section button2 is in is valid;
  // 3) the section button3 is in is valid.
  // There is no separator after button3 because there is no valid next section.
  EXPECT_EQ(menu->GetItemCount(), static_cast<size_t>(4));
  EXPECT_EQ(menu->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(0), u"DummyButton1");
  EXPECT_EQ(menu->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(1), u"DummyButton2");
  EXPECT_EQ(menu->GetTypeAt(2), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(menu->GetTypeAt(3), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(3), u"DummyButton3");
}

TEST_F(ToolbarControllerUnitTest, InValidFirstSectionAddsNoLeadingSeparator) {
  auto test_delegate = std::make_unique<TestDelegate>();
  std::unique_ptr<ToolbarController> test_controller =
      std::make_unique<TestToolbarController>(
          std::vector<ToolbarController::ResponsiveElementInfo>(
              {{ToolbarController::ElementIdInfo{kDummyButton1, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                true},
               {ToolbarController::ElementIdInfo{kDummyButton2, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                true},
               {ToolbarController::ElementIdInfo{kDummyButton3, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                true}}),
          std::vector<ui::ElementIdentifier>(
              {kDummyButton3, kDummyButton2, kDummyButton1}),
          kElementFlexOrderStart, toolbar_container_view(),
          const_cast<OverflowButton*>(overflow_button()), test_delegate.get());

  widget()->SetSize(kButtonSize);
  UpdateOverflowButtonVisibility();
  EXPECT_TRUE(overflow_button()->GetVisible());

  views::View* button1 = test_buttons()[0];
  views::View* button2 = test_buttons()[1];
  views::View* button3 = test_buttons()[2];

  // Button2 and 3 overflowed.
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_FALSE(button2->GetVisible());
  EXPECT_FALSE(button3->GetVisible());
  EXPECT_EQ(GetOverflowedElements().size(), static_cast<size_t>(2));
  const auto menu = test_controller->CreateOverflowMenuModel();
  EXPECT_TRUE(menu);

  // The first section (contains Button1) is invalid. It should not add a
  // separator before Button2.
  EXPECT_EQ(menu->GetItemCount(), static_cast<size_t>(3));
  EXPECT_EQ(menu->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(0), u"DummyButton2");
  EXPECT_EQ(menu->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(menu->GetTypeAt(2), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(2), u"DummyButton3");
}

TEST_F(ToolbarControllerUnitTest, InValidSectionInMiddleAddsNoExtraSeparator) {
  auto test_delegate = std::make_unique<TestDelegate>();
  std::unique_ptr<ToolbarController> test_controller =
      std::make_unique<TestToolbarController>(
          std::vector<ToolbarController::ResponsiveElementInfo>(
              {{ToolbarController::ElementIdInfo{kDummyButton1, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                true},
               {ToolbarController::ElementIdInfo{kDummyButton2, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                true},
               {ToolbarController::ElementIdInfo{kDummyButton3, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                true}}),
          std::vector<ui::ElementIdentifier>(
              {kDummyButton1, kDummyButton3, kDummyButton2}),
          kElementFlexOrderStart, toolbar_container_view(),
          const_cast<OverflowButton*>(overflow_button()), test_delegate.get());

  widget()->SetSize(kButtonSize);
  UpdateOverflowButtonVisibility();
  EXPECT_TRUE(overflow_button()->GetVisible());

  views::View* button1 = test_buttons()[0];
  views::View* button2 = test_buttons()[1];
  views::View* button3 = test_buttons()[2];

  // Button1 and 3 overflowed.
  EXPECT_FALSE(button1->GetVisible());
  EXPECT_TRUE(button2->GetVisible());
  EXPECT_FALSE(button3->GetVisible());
  EXPECT_EQ(GetOverflowedElements().size(), static_cast<size_t>(2));
  const auto menu = test_controller->CreateOverflowMenuModel();
  EXPECT_TRUE(menu);

  // The second section (contains Button2) is invalid. It should not add a
  // redundant separator.
  EXPECT_EQ(menu->GetItemCount(), static_cast<size_t>(3));
  EXPECT_EQ(menu->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(0), u"DummyButton1");
  EXPECT_EQ(menu->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(menu->GetTypeAt(2), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(2), u"DummyButton3");
}

TEST_F(ToolbarControllerUnitTest, InValidLastSectionAddsNoTrailingSeparator) {
  auto test_delegate = std::make_unique<TestDelegate>();
  std::unique_ptr<ToolbarController> test_controller =
      std::make_unique<TestToolbarController>(
          std::vector<ToolbarController::ResponsiveElementInfo>(
              {{ToolbarController::ElementIdInfo{kDummyButton1, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                true},
               {ToolbarController::ElementIdInfo{kDummyButton2, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                true},
               {ToolbarController::ElementIdInfo{kDummyButton3, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                true}}),
          std::vector<ui::ElementIdentifier>(
              {kDummyButton1, kDummyButton2, kDummyButton3}),
          kElementFlexOrderStart, toolbar_container_view(),
          const_cast<OverflowButton*>(overflow_button()), test_delegate.get());

  widget()->SetSize(kButtonSize);
  UpdateOverflowButtonVisibility();
  EXPECT_TRUE(overflow_button()->GetVisible());

  views::View* button1 = test_buttons()[0];
  views::View* button2 = test_buttons()[1];
  views::View* button3 = test_buttons()[2];

  // Button1 and 2 overflowed.
  EXPECT_FALSE(button1->GetVisible());
  EXPECT_FALSE(button2->GetVisible());
  EXPECT_TRUE(button3->GetVisible());
  EXPECT_EQ(GetOverflowedElements().size(), static_cast<size_t>(2));
  const auto menu = test_controller->CreateOverflowMenuModel();
  EXPECT_TRUE(menu);

  // The third section (contains Button3) is invalid. It should not add a
  // redundant trailing separator.
  EXPECT_EQ(menu->GetItemCount(), static_cast<size_t>(3));
  EXPECT_EQ(menu->GetTypeAt(0), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(0), u"DummyButton1");
  EXPECT_EQ(menu->GetTypeAt(1), ui::MenuModel::ItemType::TYPE_SEPARATOR);
  EXPECT_EQ(menu->GetTypeAt(2), ui::MenuModel::ItemType::TYPE_COMMAND);
  EXPECT_EQ(menu->GetLabelAt(2), u"DummyButton2");
}

TEST_F(ToolbarControllerUnitTest, PopOutState) {
  auto& pop_out_state = toolbar_controller()->pop_out_state_for_testing();
  EXPECT_FALSE(pop_out_state.at(kDummyButton1)->original_spec.has_value());
  EXPECT_FALSE(pop_out_state.at(kDummyButton2)->original_spec.has_value());
  EXPECT_FALSE(pop_out_state.at(kDummyButton3)->original_spec.has_value());
  EXPECT_EQ(pop_out_state.at(kDummyButton1)->responsive_spec.order(),
            kElementFlexOrderStart);
  EXPECT_EQ(pop_out_state.at(kDummyButton2)->responsive_spec.order(),
            kElementFlexOrderStart + 1);
  EXPECT_EQ(pop_out_state.at(kDummyButton3)->responsive_spec.order(),
            kElementFlexOrderStart + 2);
}

TEST_F(ToolbarControllerUnitTest, PopOutButton) {
  views::View* button1 = test_buttons()[0];
  views::View* button2 = test_buttons()[1];
  views::View* button3 = test_buttons()[2];

  // Enough space to accommodate 3 buttons.
  widget()->SetSize(gfx::Size(kButtonSize.width() * test_buttons().size(),
                              kButtonSize.height()));
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_TRUE(button2->GetVisible());
  EXPECT_TRUE(button3->GetVisible());

  // Not enough space. Button3 is hidden.
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 1),
                              kButtonSize.height()));
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_TRUE(button2->GetVisible());
  EXPECT_FALSE(button3->GetVisible());

  // Pop out button3. Button2 is hidden.
  EXPECT_TRUE(toolbar_controller()->PopOut(kDummyButton3));
  views::test::RunScheduledLayout(toolbar_container_view());
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_FALSE(button2->GetVisible());
  EXPECT_TRUE(button3->GetVisible());

  // Button3 is already popped out.
  EXPECT_FALSE(toolbar_controller()->PopOut(kDummyButton3));

  // End button3 pop out. Button3 is hidden again.
  EXPECT_TRUE(toolbar_controller()->EndPopOut(kDummyButton3));
  views::test::RunScheduledLayout(toolbar_container_view());
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_TRUE(button2->GetVisible());
  EXPECT_FALSE(button3->GetVisible());

  // Button3 already ended popped out.
  EXPECT_FALSE(toolbar_controller()->EndPopOut(kDummyButton3));

  // kDummyButton4 does not exist.
  EXPECT_FALSE(toolbar_controller()->PopOut(kDummyButton4));
}

// Buttons overflow in order: 3, 2, 1.
TEST_F(ToolbarControllerUnitTest, ButtonsOverflowRightToLeftInContainer) {
  views::View* button1 = test_buttons()[0];
  views::View* button2 = test_buttons()[1];
  views::View* button3 = test_buttons()[2];

  // Enough space to accommodate 3 buttons.
  widget()->SetSize(gfx::Size(kButtonSize.width() * test_buttons().size(),
                              kButtonSize.height()));
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_TRUE(button2->GetVisible());
  EXPECT_TRUE(button3->GetVisible());

  // Not enough space. Button3 is hidden.
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 1),
                              kButtonSize.height()));
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_TRUE(button2->GetVisible());
  EXPECT_FALSE(button3->GetVisible());

  // Keep resizing smaller. Button2 is hidden.
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 2),
                              kButtonSize.height()));
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_FALSE(button2->GetVisible());
  EXPECT_FALSE(button3->GetVisible());

  // Keep resizing smaller. Button1 is hidden.
  widget()->SetSize(gfx::Size(1, kButtonSize.height()));
  EXPECT_FALSE(button1->GetVisible());
  EXPECT_FALSE(button2->GetVisible());
  EXPECT_FALSE(button3->GetVisible());
}

// Buttons overflow in order: 1, 2, 3.
TEST_F(ToolbarControllerUnitTest, ButtonsOverflowLeftToRightInContainer) {
  auto test_delegate = std::make_unique<TestDelegate>();
  std::unique_ptr<ToolbarController> dummy_controller =
      std::make_unique<TestToolbarController>(
          std::vector<ToolbarController::ResponsiveElementInfo>(
              {{ToolbarController::ElementIdInfo{kDummyButton1, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                false},
               {ToolbarController::ElementIdInfo{kDummyButton2, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                false},
               {ToolbarController::ElementIdInfo{kDummyButton3, 0,
                                                 &vector_icons::kErrorIcon,
                                                 kDummyActivateView},
                false}}),
          std::vector<ui::ElementIdentifier>(
              {kDummyButton1, kDummyButton2, kDummyButton3}),
          kElementFlexOrderStart, toolbar_container_view(),
          const_cast<OverflowButton*>(overflow_button()), test_delegate.get());

  views::View* button1 = test_buttons()[0];
  views::View* button2 = test_buttons()[1];
  views::View* button3 = test_buttons()[2];

  // Enough space to accommodate 3 buttons.
  widget()->SetSize(gfx::Size(kButtonSize.width() * test_buttons().size(),
                              kButtonSize.height()));
  EXPECT_TRUE(button1->GetVisible());
  EXPECT_TRUE(button2->GetVisible());
  EXPECT_TRUE(button3->GetVisible());

  // Not enough space. Button1 is hidden.
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 1),
                              kButtonSize.height()));
  EXPECT_FALSE(button1->GetVisible());
  EXPECT_TRUE(button2->GetVisible());
  EXPECT_TRUE(button3->GetVisible());

  // Keep resizing smaller. Button2 is hidden.
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 2),
                              kButtonSize.height()));
  EXPECT_FALSE(button1->GetVisible());
  EXPECT_FALSE(button2->GetVisible());
  EXPECT_TRUE(button3->GetVisible());

  // Keep resizing smaller. Button3 is hidden.
  widget()->SetSize(gfx::Size(1, kButtonSize.height()));
  EXPECT_FALSE(button1->GetVisible());
  EXPECT_FALSE(button2->GetVisible());
  EXPECT_FALSE(button3->GetVisible());
}

TEST_F(ToolbarControllerUnitTest, MenuItemUsability) {
  views::View* button3 = test_buttons()[2];
  button3->SetEnabled(false);

  // Not enough space. Button3 is hidden.
  widget()->SetSize(gfx::Size(kButtonSize.width() * (test_buttons().size() - 1),
                              kButtonSize.height()));
  UpdateOverflowButtonVisibility();
  EXPECT_TRUE(overflow_button()->GetVisible());
  EXPECT_FALSE(button3->GetVisible());

  widget()->LayoutRootViewIfNecessary();
  event_generator()->MoveMouseTo(
      overflow_button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();

  const ui::SimpleMenuModel* menu = overflow_menu();
  const auto overflowed_buttons = GetOverflowedElements();

  EXPECT_TRUE(menu);
  const auto& responsive_elements = GetResponsiveElements(toolbar_controller());
  for (size_t i = 0; i < responsive_elements.size(); ++i) {
    if (IsOverflowed(responsive_elements[i])) {
      EXPECT_EQ(ToolbarController::FindToolbarElementWithId(
                    toolbar_container_view(),
                    absl::get<ToolbarController::ElementIdInfo>(
                        responsive_elements[i].overflow_id)
                        .overflow_identifier)
                    ->GetEnabled(),
                menu->IsEnabledAt(menu->GetIndexOfCommandId(i).value()));
    }
  }
}

TEST_F(ToolbarControllerUnitTest, SupportActionIds) {
  auto test_delegate = std::make_unique<TestDelegate>();
  auto test_controller = std::make_unique<ToolbarController>(
      std::vector<ToolbarController::ResponsiveElementInfo>(
          {{test_delegate->get_action_ids()[0]},
           {test_delegate->get_action_ids()[1]},
           {test_delegate->get_action_ids()[2]}}),
      std::vector<ui::ElementIdentifier>(), kElementFlexOrderStart,
      toolbar_container_view(), const_cast<OverflowButton*>(overflow_button()),
      test_delegate.get());
  test_delegate->SetContainerView(
      toolbar_container_view()->AddChildView(std::make_unique<views::View>()));

  toolbar_controller()->overflow_button()->SetVisible(
      test_controller->ShouldShowOverflowButton(widget()->GetSize()));
  EXPECT_TRUE(overflow_button()->GetVisible());

  const auto menu = test_controller->CreateOverflowMenuModel();
  EXPECT_TRUE(menu);
  EXPECT_EQ(menu->GetItemCount(),
            static_cast<size_t>(test_delegate->get_overflowed_count()));

  // Overflowed actions should match overflow menu.
  const auto& responsive_elements =
      GetResponsiveElements(test_controller.get());
  for (size_t i = 0; i < responsive_elements.size(); ++i) {
    if (IsOverflowed(responsive_elements[i])) {
      size_t index = menu->GetIndexOfCommandId(i).value();
      EXPECT_EQ(test_controller->GetMenuText(responsive_elements[i]),
                menu->GetLabelAt(index));
      EXPECT_CALL(*test_delegate, DummyAction);
      menu->ActivatedAt(index);
    }
  }
}

TEST_F(ToolbarControllerUnitTest, StatusIndicatorVisibilityUpdates) {
  auto test_delegate = std::make_unique<TestDelegate>();
  auto test_controller = std::make_unique<ToolbarController>(
      std::vector<ToolbarController::ResponsiveElementInfo>(
          {{test_delegate->get_action_ids()[0]},
           {test_delegate->get_action_ids()[1]},
           {test_delegate->get_action_ids()[2]}}),
      std::vector<ui::ElementIdentifier>(), kElementFlexOrderStart,
      toolbar_container_view(), const_cast<OverflowButton*>(overflow_button()),
      test_delegate.get());
  test_delegate->SetContainerView(
      toolbar_container_view()->AddChildView(std::make_unique<views::View>()));

  toolbar_controller()->overflow_button()->SetVisible(
      test_controller->ShouldShowOverflowButton(widget()->GetSize()));
  EXPECT_TRUE(overflow_button()->GetVisible());

  overflow_button()->set_toolbar_controller(test_controller.get());

  widget()->LayoutRootViewIfNecessary();
  overflow_button()->RunMenu();

  const ui::SimpleMenuModel* menu = test_controller->menu_model_for_testing();

  // Overflowed buttons should match overflow menu.
  EXPECT_TRUE(menu);
  views::SubmenuView* sub_menu =
      test_controller->root_menu_item()->GetSubmenu();

  for (auto* menu_item : sub_menu->GetMenuItems()) {
    PinnedToolbarButtonStatusIndicator* status_indicator =
        PinnedToolbarButtonStatusIndicator::GetStatusIndicator(
            menu_item->icon_view());
    EXPECT_TRUE(status_indicator);
    EXPECT_EQ(status_indicator->GetVisible(), true);
  }

  const auto& responsive_elements =
      GetResponsiveElements(test_controller.get());
  for (size_t i = 0; i < responsive_elements.size(); ++i) {
    if (IsOverflowed(responsive_elements[i])) {
      actions::ActionId element_action_id =
          absl::get<actions::ActionId>(responsive_elements[i].overflow_id);
      test_delegate->GetActionItemFor(element_action_id)
          ->SetProperty(kActionItemUnderlineIndicatorKey, false);

      size_t index = menu->GetIndexOfCommandId(i).value();

      views::MenuItemView* menu_item = sub_menu->GetMenuItemAt(index);
      PinnedToolbarButtonStatusIndicator* status_indicator =
          PinnedToolbarButtonStatusIndicator::GetStatusIndicator(
              menu_item->icon_view());
      EXPECT_EQ(status_indicator->GetVisible(), false);
    }
  }

  for (size_t i = 0; i < responsive_elements.size(); ++i) {
    if (IsOverflowed(responsive_elements[i])) {
      actions::ActionId element_action_id =
          absl::get<actions::ActionId>(responsive_elements[i].overflow_id);
      test_delegate->GetActionItemFor(element_action_id)
          ->SetProperty(kActionItemUnderlineIndicatorKey, true);

      size_t index = menu->GetIndexOfCommandId(i).value();

      views::MenuItemView* menu_item = sub_menu->GetMenuItemAt(index);
      PinnedToolbarButtonStatusIndicator* status_indicator =
          PinnedToolbarButtonStatusIndicator::GetStatusIndicator(
              menu_item->icon_view());
      EXPECT_EQ(status_indicator->GetVisible(), true);
    }
  }

  overflow_button()->set_toolbar_controller(nullptr);
}
