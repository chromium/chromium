// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/views/help_bubble_factory_mac.h"

#import <AppKit/AppKit.h>

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_education/common/help_bubble/help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/views/help_bubble_views.h"
#include "components/user_education/views/help_bubble_views_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace user_education {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestRootViewId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestMenuItemId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestSubmenuItemId);

constexpr gfx::Rect kWidgetBounds(10, 20, 900, 700);
constexpr gfx::Rect kMenuItemBounds(171, 345, 325, 29);

}  // namespace

class HelpBubbleFactoryMacTest : public views::ViewsTestBase {
 public:
  HelpBubbleFactoryMacTest() = default;
  ~HelpBubbleFactoryMacTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = std::make_unique<test::TestThemedWidget>();
    widget_->Init(CreateParamsForTestWidget());
    contents_view_ = widget_->SetContentsView(std::make_unique<views::View>());
    contents_view_->SetLayoutManager(std::make_unique<views::FillLayout>());
    contents_view_->SetProperty(views::kElementIdentifierKey, kTestRootViewId);
    widget_->SetBounds(kWidgetBounds);
    views::test::WidgetVisibleWaiter waiter(widget_.get());
    widget_->Show();
    waiter.Wait();

    root_menu_ = [[NSMenu alloc] initWithTitle:@"ContextMenu"];
    menu_item_ = [[NSMenuItem alloc] initWithTitle:@"Send to your devices"
                                            action:nil
                                     keyEquivalent:@""];
    submenu_ = [[NSMenu alloc] initWithTitle:@"SendToYourDevicesSubmenu"];
    [root_menu_ addItem:menu_item_];
    [root_menu_ setSubmenu:submenu_ forItem:menu_item_];

    ui::ElementTrackerMac::GetInstance()->NotifyMenuWillShow(root_menu_,
                                                             GetContext());
    menu_showing_ = true;
  }

  void TearDown() override {
    if (menu_showing_) {
      if (submenu_item_shown_) {
        ui::ElementTrackerMac::GetInstance()->NotifyMenuItemHidden(
            submenu_, kTestSubmenuItemId);
        submenu_item_shown_ = false;
      }
      if (menu_item_shown_) {
        ui::ElementTrackerMac::GetInstance()->NotifyMenuItemHidden(
            root_menu_, kTestMenuItemId);
        menu_item_shown_ = false;
      }
      ui::ElementTrackerMac::GetInstance()->NotifyMenuDoneShowing(root_menu_);
      menu_showing_ = false;
    }
    contents_view_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  ui::ElementContext GetContext() const {
    return views::ElementTrackerViews::GetContextForWidget(widget_.get());
  }

  ui::TrackedElement* ShowMenuItem(const gfx::Rect& bounds = kMenuItemBounds) {
    ui::ElementTrackerMac::GetInstance()->NotifyMenuItemShown(
        root_menu_, kTestMenuItemId, bounds);
    menu_item_shown_ = true;
    return ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
        kTestMenuItemId, GetContext());
  }

  void HideMenuItem() {
    if (menu_item_shown_) {
      ui::ElementTrackerMac::GetInstance()->NotifyMenuItemHidden(
          root_menu_, kTestMenuItemId);
      menu_item_shown_ = false;
    }
  }

  ui::TrackedElement* ShowSubmenuItem(const gfx::Rect& bounds) {
    ui::ElementTrackerMac::GetInstance()->NotifyMenuItemShown(
        submenu_, kTestSubmenuItemId, bounds);
    submenu_item_shown_ = true;
    return ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
        kTestSubmenuItemId, GetContext());
  }

  void HideSubmenuItem() {
    if (submenu_item_shown_) {
      ui::ElementTrackerMac::GetInstance()->NotifyMenuItemHidden(
          submenu_, kTestSubmenuItemId);
      submenu_item_shown_ = false;
    }
  }

  std::unique_ptr<HelpBubble> CreateHelpBubble(
      ui::TrackedElement* element,
      HelpBubbleArrow arrow = HelpBubbleArrow::kBottomLeft) {
    HelpBubbleParams params;
    params.body_text = u"Hover over Send to your devices and pick a device";
    params.arrow = arrow;
    auto bubble = factory_.CreateBubble(element, std::move(params));
    if (bubble) {
      views::test::RunScheduledLayout(
          bubble->AsA<HelpBubbleViews>()->bubble_view_for_testing());
    }
    return bubble;
  }

  test::TestHelpBubbleDelegate test_delegate_;
  HelpBubbleFactoryMac factory_{&test_delegate_};
  raw_ptr<views::View> contents_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  NSMenu* __strong root_menu_ = nil;
  NSMenuItem* __strong menu_item_ = nil;
  NSMenu* __strong submenu_ = nil;
  bool menu_showing_ = false;
  bool menu_item_shown_ = false;
  bool submenu_item_shown_ = false;
};

TEST_F(HelpBubbleFactoryMacTest, CanBuildBubbleForTrackedElement) {
  ui::TrackedElement* const mac_element = ShowMenuItem();
  ASSERT_NE(nullptr, mac_element);
  EXPECT_TRUE(factory_.CanBuildBubbleForTrackedElement(mac_element));

  views::TrackedElementViews* const views_element =
      views::ElementTrackerViews::GetInstance()->GetElementForView(
          contents_view_);
  ASSERT_NE(nullptr, views_element);
  EXPECT_FALSE(factory_.CanBuildBubbleForTrackedElement(views_element));
}

TEST_F(HelpBubbleFactoryMacTest, BottomLeftBubblePositionedAboveMenuItem) {
  if (@available(macOS 14.0, *)) {
  } else {
    GTEST_SKIP() << "Custom menu item bounds require macOS 14.0+";
  }

  ui::TrackedElement* const mac_element = ShowMenuItem(kMenuItemBounds);
  ASSERT_NE(nullptr, mac_element);

  auto help_bubble =
      CreateHelpBubble(mac_element, HelpBubbleArrow::kBottomLeft);
  ASSERT_NE(nullptr, help_bubble);

  auto* const bubble_view =
      help_bubble->AsA<HelpBubbleViews>()->bubble_view_for_testing();
  ASSERT_NE(nullptr, bubble_view);

  const gfx::Rect bubble_bounds = help_bubble->GetBoundsInScreen();
  // With `kBottomLeft`, the bubble sits strictly above the anchor menu item
  // and above the submenu that opens beside it, leaving both unobstructed.
  EXPECT_LE(bubble_bounds.bottom(), kMenuItemBounds.y());
  EXPECT_FALSE(bubble_bounds.Intersects(kMenuItemBounds));

  // The submenu appears adjacent to the root menu item.
  constexpr int kSubmenuGap = 24;
  const gfx::Rect submenu_item_bounds(kMenuItemBounds.right() + kSubmenuGap,
                                      kMenuItemBounds.y(), 220,
                                      kMenuItemBounds.height());
  EXPECT_FALSE(bubble_bounds.Intersects(submenu_item_bounds));

  ui::TrackedElement* const submenu_element =
      ShowSubmenuItem(submenu_item_bounds);
  ASSERT_NE(nullptr, submenu_element);
  EXPECT_TRUE(help_bubble->is_open());

  HideSubmenuItem();
  EXPECT_TRUE(help_bubble->is_open());
}

TEST_F(HelpBubbleFactoryMacTest, HelpBubbleDismissedOnMenuItemHidden) {
  UNCALLED_MOCK_CALLBACK(HelpBubble::ClosingCallback, closing);
  UNCALLED_MOCK_CALLBACK(HelpBubble::ClosedCallback, closed);

  ui::TrackedElement* const mac_element = ShowMenuItem();
  ASSERT_NE(nullptr, mac_element);

  auto help_bubble = CreateHelpBubble(mac_element);
  ASSERT_NE(nullptr, help_bubble);
  auto closing_sub = help_bubble->AddOnClosingCallback(closing.Get());
  auto closed_sub = help_bubble->AddOnClosedCallback(closed.Get());

  EXPECT_CALLS_IN_SCOPE_2(
      closing, Run(help_bubble.get(), HelpBubble::CloseReason::kAnchorHidden),
      closed, Run(HelpBubble::CloseReason::kAnchorHidden), HideMenuItem());
  EXPECT_FALSE(help_bubble->is_open());
}

class HelpBubbleFactoryMacWithRaiseMacHelpBubbleAboveMenusEnabledTest
    : public HelpBubbleFactoryMacTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      features::kRaiseMacHelpBubbleAboveMenus};
};

TEST_F(HelpBubbleFactoryMacWithRaiseMacHelpBubbleAboveMenusEnabledTest,
       CreateBubbleElevatesWindowLevelAbovePopUpMenu) {
  ui::TrackedElement* const mac_element = ShowMenuItem();
  ASSERT_NE(nullptr, mac_element);

  auto help_bubble = CreateHelpBubble(mac_element);
  ASSERT_NE(nullptr, help_bubble);
  EXPECT_TRUE(help_bubble->is_open());

  auto* const bubble_view =
      help_bubble->AsA<HelpBubbleViews>()->bubble_view_for_testing();
  ASSERT_NE(nullptr, bubble_view);
  views::Widget* const bubble_widget = bubble_view->GetWidget();
  ASSERT_NE(nullptr, bubble_widget);
  EXPECT_TRUE(bubble_widget->IsVisible());

  if (@available(macOS 14.0, *)) {
    EXPECT_EQ(kMenuItemBounds, bubble_view->GetAnchorRect());
  }

  NSWindow* const bubble_ns_window =
      bubble_widget->GetNativeWindow().GetNativeNSWindow();
  ASSERT_NE(nil, bubble_ns_window);
  EXPECT_EQ(NSPopUpMenuWindowLevel + 1, [bubble_ns_window level]);

  // Verify the parent browser widget's window level remains normal and the
  // bubble window preserves transient child collection behavior.
  NSWindow* const parent_ns_window =
      widget_->GetNativeWindow().GetNativeNSWindow();
  ASSERT_NE(nil, parent_ns_window);
  EXPECT_EQ(NSNormalWindowLevel, [parent_ns_window level]);
  EXPECT_NE(0u, [bubble_ns_window collectionBehavior] &
                    NSWindowCollectionBehaviorTransient);
  EXPECT_EQ(0u, [bubble_ns_window collectionBehavior] &
                    NSWindowCollectionBehaviorManaged);
}

class HelpBubbleFactoryMacWithRaiseMacHelpBubbleAboveMenusDisabledTest
    : public HelpBubbleFactoryMacTest {
 public:
  HelpBubbleFactoryMacWithRaiseMacHelpBubbleAboveMenusDisabledTest() {
    feature_list_.InitAndDisableFeature(
        features::kRaiseMacHelpBubbleAboveMenus);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that disabling kRaiseMacHelpBubbleAboveMenus preserves legacy behavior:
// help bubbles remain at normal window level.
TEST_F(HelpBubbleFactoryMacWithRaiseMacHelpBubbleAboveMenusDisabledTest,
       CreateBubblePreservesNormalWindowLevel) {
  ui::TrackedElement* const mac_element = ShowMenuItem();
  ASSERT_NE(nullptr, mac_element);

  auto help_bubble = CreateHelpBubble(mac_element);
  ASSERT_NE(nullptr, help_bubble);
  EXPECT_TRUE(help_bubble->is_open());

  auto* const bubble_view =
      help_bubble->AsA<HelpBubbleViews>()->bubble_view_for_testing();
  ASSERT_NE(nullptr, bubble_view);
  views::Widget* const bubble_widget = bubble_view->GetWidget();
  ASSERT_NE(nullptr, bubble_widget);
  EXPECT_TRUE(bubble_widget->IsVisible());

  NSWindow* const bubble_ns_window =
      bubble_widget->GetNativeWindow().GetNativeNSWindow();
  ASSERT_NE(nil, bubble_ns_window);
  EXPECT_EQ(NSNormalWindowLevel, [bubble_ns_window level]);

  HideMenuItem();
}

}  // namespace user_education
