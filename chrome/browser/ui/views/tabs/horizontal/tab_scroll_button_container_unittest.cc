// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/horizontal/tab_scroll_button_container.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_solid_color.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/test/ink_drop_impl_test_api.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

using TabScrollButtonContainerTest = ChromeViewsTestBase;

TEST_F(TabScrollButtonContainerTest,
       ScrollButtonsHaveExpectedColorsWhenActiveAndInactive) {
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetContentsView(
      std::make_unique<TabScrollButtonContainer>(&browser_window_interface));
  widget->ShowInactive();
  const ui::ElementContext context =
      views::ElementTrackerViews::GetContextForWidget(widget.get());
  auto* start_button =
      views::ElementTrackerViews::GetInstance()
          ->GetFirstMatchingViewAs<TabStripControlButton>(
              TabScrollButtonContainer::kStartScrollButton, context);
  auto* end_button =
      views::ElementTrackerViews::GetInstance()
          ->GetFirstMatchingViewAs<TabStripControlButton>(
              TabScrollButtonContainer::kEndScrollButton, context);
  ASSERT_NE(start_button, nullptr);
  ASSERT_NE(end_button, nullptr);

  auto active_lock = widget->LockPaintAsActive();
  ASSERT_TRUE(widget->ShouldPaintAsActive());
  views::InkDrop::Get(start_button)->GetInkDrop()->SetHovered(true);
  views::InkDrop::Get(end_button)->GetInkDrop()->SetHovered(true);
  views::test::InkDropImplTestApi start_ink_drop_api(
      static_cast<views::InkDropImpl*>(
          views::InkDrop::Get(start_button)->GetInkDrop()));
  views::test::InkDropImplTestApi end_ink_drop_api(
      static_cast<views::InkDropImpl*>(
          views::InkDrop::Get(end_button)->GetInkDrop()));
  ASSERT_NE(start_ink_drop_api.highlight(), nullptr);
  ASSERT_NE(end_ink_drop_api.highlight(), nullptr);

  const ui::ColorProvider* color_provider = widget->GetColorProvider();
  const SkColor expected_active_bg_color =
      color_provider->GetColor(kColorNewTabButtonBackgroundFrameActive);
  const SkColor expected_active_fg_color =
      color_provider->GetColor(kColorTabForegroundInactiveFrameActive);
  const SkColor expected_active_hover_color =
      SkColorSetA(color_provider->GetColor(kColorTabStripControlButtonInkDrop),
                  SK_AlphaOPAQUE);
  EXPECT_EQ(start_button->GetBackgroundColor(),
            kColorNewTabButtonBackgroundFrameActive);
  EXPECT_EQ(end_button->GetBackgroundColor(),
            kColorNewTabButtonBackgroundFrameActive);
  EXPECT_EQ(color_provider->GetColor(start_button->GetBackgroundColor()),
            expected_active_bg_color);
  EXPECT_EQ(color_provider->GetColor(end_button->GetBackgroundColor()),
            expected_active_bg_color);
  EXPECT_EQ(color_provider->GetColor(start_button->GetForegroundColor()),
            expected_active_fg_color);
  EXPECT_EQ(color_provider->GetColor(end_button->GetForegroundColor()),
            expected_active_fg_color);
  EXPECT_EQ(start_ink_drop_api.highlight()
                ->layer()
                ->AsSolidColor()
                ->background_color()
                .toSkColor(),
            expected_active_hover_color);
  EXPECT_EQ(end_ink_drop_api.highlight()
                ->layer()
                ->AsSolidColor()
                ->background_color()
                .toSkColor(),
            expected_active_hover_color);

  active_lock.reset();

  ASSERT_FALSE(widget->ShouldPaintAsActive());
  ASSERT_NE(start_ink_drop_api.highlight(), nullptr);
  ASSERT_NE(end_ink_drop_api.highlight(), nullptr);
  const SkColor expected_inactive_bg_color =
      color_provider->GetColor(kColorNewTabButtonBackgroundFrameInactive);
  const SkColor expected_inactive_fg_color =
      color_provider->GetColor(kColorTabForegroundInactiveFrameInactive);
  const SkColor expected_inactive_hover_color = SkColorSetA(
      color_provider->GetColor(kColorTabStripControlButtonInkDropFrameInactive),
      SK_AlphaOPAQUE);
  EXPECT_NE(expected_active_bg_color, expected_inactive_bg_color);
  EXPECT_NE(expected_active_hover_color, expected_inactive_hover_color);
  EXPECT_EQ(start_button->GetBackgroundColor(),
            kColorNewTabButtonBackgroundFrameInactive);
  EXPECT_EQ(end_button->GetBackgroundColor(),
            kColorNewTabButtonBackgroundFrameInactive);
  EXPECT_EQ(color_provider->GetColor(start_button->GetBackgroundColor()),
            expected_inactive_bg_color);
  EXPECT_EQ(color_provider->GetColor(end_button->GetBackgroundColor()),
            expected_inactive_bg_color);
  EXPECT_EQ(color_provider->GetColor(start_button->GetForegroundColor()),
            expected_inactive_fg_color);
  EXPECT_EQ(color_provider->GetColor(end_button->GetForegroundColor()),
            expected_inactive_fg_color);
  EXPECT_EQ(start_ink_drop_api.highlight()
                ->layer()
                ->AsSolidColor()
                ->background_color()
                .toSkColor(),
            expected_inactive_hover_color);
  EXPECT_EQ(end_ink_drop_api.highlight()
                ->layer()
                ->AsSolidColor()
                ->background_color()
                .toSkColor(),
            expected_inactive_hover_color);
}

TEST_F(TabScrollButtonContainerTest,
       IphOnlyTriggersWhenContainerIsActuallyDrawn) {
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface;
  testing::NiceMock<MockBrowserUserEducationInterface> user_education(
      &browser_window_interface);
  ON_CALL(user_education, HasFeaturePromoBeenDismissed(testing::Ref(
                              feature_engagement::kIPHTabScrollButtonFeature)))
      .WillByDefault(testing::Return(false));
  ON_CALL(user_education, MaybeShowFeaturePromo(testing::_))
      .WillByDefault(testing::Return(true));

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  views::View* root_container =
      widget->SetContentsView(std::make_unique<views::View>());
  TabScrollButtonContainer* scroll_container = root_container->AddChildView(
      std::make_unique<TabScrollButtonContainer>(&browser_window_interface));
  scroll_container->SetVisible(false);

  // Showing the widget propagates a visibility change with `visible = true`
  // from the RootView even though `scroll_container` is hidden. The IPH
  // must not trigger while the container is not drawn.
  EXPECT_CALL(user_education, MaybeShowFeaturePromo(testing::_)).Times(0);
  widget->ShowInactive();
  task_environment()->FastForwardBy(base::Seconds(5));
  testing::Mock::VerifyAndClearExpectations(&user_education);

  // Hiding before the delay elapses should also cancel the IPH timer.
  scroll_container->SetVisible(true);
  task_environment()->FastForwardBy(base::Seconds(1));
  scroll_container->SetVisible(false);
  EXPECT_CALL(user_education, MaybeShowFeaturePromo(testing::_)).Times(0);
  task_environment()->FastForwardBy(base::Seconds(5));
  testing::Mock::VerifyAndClearExpectations(&user_education);

  // Keeping the container drawn for the full delay triggers the IPH.
  EXPECT_CALL(
      user_education,
      MaybeShowFeaturePromo(testing::Field(
          &user_education::FeaturePromoParams::feature,
          testing::Property(
              &raw_ref<const base::Feature>::get,
              testing::Ref(feature_engagement::kIPHTabScrollButtonFeature)))))
      .Times(1);
  scroll_container->SetVisible(true);
  task_environment()->FastForwardBy(base::Seconds(5));
}
