// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/fake_tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/content_settings/core/common/features.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

using views::Widget;

class TabTest : public ChromeViewsTestBase {
 public:
  TabTest() {
    // Prevent the fake clock from starting at 0 which is the null time.
    fake_clock_.Advance(base::Milliseconds(2000));
  }
  ~TabTest() override = default;

  static TabIcon* GetTabIcon(Tab* tab) { return tab->icon_; }

  static views::Label* GetTabTitle(Tab* tab) { return tab->title_; }

  static views::ImageButton* GetAlertIndicator(Tab* tab) {
    return tab->alert_indicator_button_;
  }

  static TabCloseButton* GetCloseButton(Tab* tab) { return tab->close_button_; }

  static int GetTitleWidth(Tab* tab) { return tab->title_->bounds().width(); }

  static void EndTitleAnimation(Tab* tab) { tab->title_animation_.End(); }

  static void LayoutTab(Tab* tab) { views::test::RunScheduledLayout(tab); }

  static int VisibleIconCount(const Tab& tab) {
    return tab.showing_icon_ + tab.showing_alert_indicator_ +
           tab.showing_close_button_;
  }

  static void CheckForExpectedLayoutAndVisibilityOfElements(const Tab& tab) {
    // Check whether elements are visible when they are supposed to be, given
    // Tab size and TabRendererData state.
    if (tab.data_.pinned) {
      EXPECT_EQ(1, VisibleIconCount(tab));
      if (tab.data_.alert_state.size()) {
        EXPECT_FALSE(tab.showing_icon_);
        EXPECT_TRUE(tab.showing_alert_indicator_);
      } else {
        EXPECT_TRUE(tab.showing_icon_);
        EXPECT_FALSE(tab.showing_alert_indicator_);
      }
      EXPECT_FALSE(tab.title_->GetVisible());
      EXPECT_FALSE(tab.showing_close_button_);
    } else if (tab.IsActive()) {
      EXPECT_TRUE(tab.showing_close_button_);
      switch (VisibleIconCount(tab)) {
        case 1:
          EXPECT_FALSE(tab.showing_icon_);
          EXPECT_FALSE(tab.showing_alert_indicator_);
          break;
        case 2:
          if (tab.data_.alert_state.size()) {
            EXPECT_FALSE(tab.showing_icon_);
            EXPECT_TRUE(tab.showing_alert_indicator_);
          } else {
            EXPECT_TRUE(tab.showing_icon_);
            EXPECT_FALSE(tab.showing_alert_indicator_);
          }
          break;
        default:
          EXPECT_EQ(3, VisibleIconCount(tab));
          EXPECT_FALSE(tab.data_.alert_state.empty());
          break;
      }
    } else {  // Tab not active and not pinned tab.
      switch (VisibleIconCount(tab)) {
        case 1:
          EXPECT_FALSE(tab.showing_close_button_);
          if (tab.data_.alert_state.empty()) {
            EXPECT_FALSE(tab.showing_alert_indicator_);
            EXPECT_TRUE(tab.showing_icon_);
          } else {
            EXPECT_FALSE(tab.showing_icon_);
            EXPECT_TRUE(tab.showing_alert_indicator_);
          }
          break;
        case 2:
          EXPECT_TRUE(tab.showing_icon_);
          if (tab.data_.alert_state.size())
            EXPECT_TRUE(tab.showing_alert_indicator_);
          else
            EXPECT_FALSE(tab.showing_alert_indicator_);
          break;
        default:
          EXPECT_EQ(3, VisibleIconCount(tab));
          EXPECT_FALSE(tab.data_.alert_state.empty());
      }
    }

    // Check the tab icon's positioning. Icons should be positioned at the
    // start of the tab. Favicons should be centered within their icons. We
    // extend the bounds vertically down along the tab so that the crashed tabs
    // and alerts icons can be placed. This means that the true bounds are not
    // centered on the contents bounds.
    const gfx::Rect contents_bounds = tab.GetContentsBounds();
    if (tab.showing_icon_) {
      gfx::Rect icon_bounds = tab.icon_->bounds();
      icon_bounds.Inset(tab.icon_->GetInsets());
      if (tab.center_icon_) {
        EXPECT_LE(icon_bounds.x(), contents_bounds.x());
      } else {
        EXPECT_LE(contents_bounds.x(), icon_bounds.x());
      }
      if (tab.title_->GetVisible()) {
        EXPECT_LE(tab.icon_->bounds().right(), tab.title_->x());
      }

      // Tab Icon content now exactly fit the content bounds.
      EXPECT_EQ(icon_bounds.y(), contents_bounds.y());
      EXPECT_GE(tab.icon_->bounds().bottom(), contents_bounds.bottom());
    }

    if (tab.showing_icon_ && tab.showing_alert_indicator_) {
      // When checking for overlap, other views should not overlap the main
      // favicon (covered by kFaviconSize) but can overlap the extra space
      // reserved for the attention indicator.
      int icon_visual_right = tab.icon_->bounds().x() + gfx::kFaviconSize;
      EXPECT_LE(icon_visual_right, GetAlertIndicatorBounds(tab).x());
    }

    if (tab.showing_alert_indicator_) {
      if (tab.title_->GetVisible()) {
        EXPECT_LE(tab.title_->bounds().right(),
                  GetAlertIndicatorBounds(tab).x());
      }
      if (tab.center_icon_) {
        EXPECT_LE(contents_bounds.right(),
                  GetAlertIndicatorBounds(tab).right());
      } else {
        EXPECT_LE(GetAlertIndicatorBounds(tab).right(),
                  contents_bounds.right());
      }

      // The alert indicator should be centered in the content bounds.
      gfx::Rect alert_bounds = GetAlertIndicatorBounds(tab);
      EXPECT_EQ(alert_bounds.CenterPoint().y(),
                contents_bounds.CenterPoint().y());
    }

    if (tab.showing_alert_indicator_ && tab.showing_close_button_) {
      // Note: The alert indicator can overlap the left-insets of the close box,
      // but should otherwise be to the left of the close button.
      EXPECT_LE(GetAlertIndicatorBounds(tab).right(),
                tab.close_button_->bounds().x() +
                    tab.close_button_->GetInsets().left());
    }
    if (tab.showing_close_button_) {
      // Note: The title bounds can overlap the left-insets of the close box,
      // but should otherwise be to the left of the close button.
      if (tab.title_->GetVisible()) {
        EXPECT_LE(tab.title_->bounds().right(),
                  tab.close_button_->bounds().x() +
                      tab.close_button_->GetInsets().left());
      }

      // The close button has a larger hit target than the content bounds.
      const gfx::Rect close_bounds = tab.close_button_->GetContentsBounds();
      EXPECT_LE(close_bounds.right(), tab.GetLocalBounds().right());
      EXPECT_LE(close_bounds.y(), contents_bounds.y());
      EXPECT_LE(contents_bounds.bottom(), close_bounds.bottom());
    }
  }

  static void StopFadeAnimationIfNecessary(const Tab& tab) {
    // Stop the fade animation directly instead of waiting an unknown number of
    // seconds.
    if (gfx::Animation* fade_animation =
            tab.alert_indicator_button_->fade_animation_.get()) {
      fade_animation->Stop();
    }
  }

  void SetupFakeClock(TabIcon* icon) { icon->clock_ = &fake_clock_; }

 private:
  static gfx::Rect GetAlertIndicatorBounds(const Tab& tab) {
    if (!tab.alert_indicator_button_) {
      ADD_FAILURE();
      return gfx::Rect();
    }
    return tab.alert_indicator_button_->bounds();
  }

  std::string original_locale_;
  base::SimpleTestTickClock fake_clock_;
};

class AlertIndicatorButtonTest : public ChromeViewsTestBase {
 public:
  AlertIndicatorButtonTest() = default;
  AlertIndicatorButtonTest(const AlertIndicatorButtonTest&) = delete;
  AlertIndicatorButtonTest& operator=(const AlertIndicatorButtonTest&) = delete;
  ~AlertIndicatorButtonTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    controller_ = new FakeBaseTabStripController;
    tab_strip_ = new TabStrip(std::unique_ptr<TabStripController>(controller_));
    controller_->set_tab_strip(tab_strip_);

    // The tab strip must be added to the view hierarchy for it to create the
    // buttons.
    auto parent = std::make_unique<views::View>();
    views::FlexLayout* layout_manager =
        parent->SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetDefault(
            views::kFlexBehaviorKey,
            views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                     views::MaximumFlexSizeRule::kUnbounded));
    parent->AddChildView(tab_strip_.get());

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget_->SetContentsView(std::move(parent));
  }

  void TearDown() override {
    // All windows need to be closed before tear down.
    widget_.reset();

    ChromeViewsTestBase::TearDown();
  }

 protected:
  bool showing_close_button(Tab* tab) const {
    return tab->showing_close_button_;
  }
  bool showing_icon(Tab* tab) const { return tab->showing_icon_; }
  bool showing_alert_indicator(Tab* tab) const {
    return tab->showing_alert_indicator_;
  }

  base::Time get_camera_mic_indicator_start_time(Tab* tab) {
    return tab->alert_indicator_button_->camera_mic_indicator_start_time_;
  }

  base::TimeDelta get_fadeout_animation_duration_for_testing_(Tab* tab) {
    return tab->alert_indicator_button_
        ->fadeout_animation_duration_for_testing_;
  }

  void StopAnimation(Tab* tab) {
    ASSERT_TRUE(tab->alert_indicator_button_->fade_animation_);
    tab->alert_indicator_button_->fade_animation_->Stop();
  }

  // Owned by TabStrip.
  raw_ptr<FakeBaseTabStripController, DanglingUntriaged> controller_ = nullptr;
  raw_ptr<TabStrip, DanglingUntriaged> tab_strip_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(TabTest, HitTest) {
  auto tab_slot_controller = std::make_unique<FakeTabSlotController>();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Tab* tab =
      widget->SetContentsView(std::make_unique<Tab>(tab_slot_controller.get()));
  tab->SizeToPreferredSize();

  // Attempt to click on the left curved extender. this is not a part of the
  // hit target.
  // x ╭─────────╮
  //   │ Content │
  // ┏─╯         ╰─┐
  int middle_y = tab->height() / 2;
  EXPECT_FALSE(tab->HitTestPoint(gfx::Point(0, middle_y)));

  // Attempt to click above the tab. this is not a part of the hit target.
  //        x
  //   ╭─────────╮
  //   │ Content │
  // ┏─╯         ╰─┐
  int middle_x = tab->width() / 2;
  EXPECT_FALSE(tab->HitTestPoint(gfx::Point(middle_x, -1)));

  int tab_starting_y =
      GetLayoutConstant(TAB_STRIP_HEIGHT) - GetLayoutConstant(TAB_HEIGHT);

  // Attempt to click on the top pixel of the tab. This should be part of the
  // hit target.
  //   ╭────x────╮
  //   │ Content │
  // ┏─╯         ╰─┐
  EXPECT_TRUE(tab->HitTestPoint(gfx::Point(middle_x, tab_starting_y)));

  // In maximized mode, attempt to click on the top pixel of the tab. This
  // should be part of the hit target.
  //   ╭────x────╮
  //   │ Content │
  // ┏─╯         ╰─┐
  widget->Maximize();
  EXPECT_TRUE(tab->HitTestPoint(gfx::Point(middle_x, tab_starting_y)));

  // Attempt to click on the left curved extender. this is not a part of the
  // hit target.
  // x ╭─────────╮
  //   │ Content │
  // ┏─╯         ╰─┐
  EXPECT_FALSE(tab->HitTestPoint(gfx::Point(0, tab_starting_y)));

  // Attempt to click on the right curved extender. this is not a part of the
  // hit target.
  //   ╭─────────╮ x
  //   │ Content │
  // ┏─╯         ╰─┐
  EXPECT_FALSE(tab->HitTestPoint(gfx::Point(tab->width() - 1, tab_starting_y)));
}

TEST_F(TabTest, LayoutAndVisibilityOfElements) {
  static const std::optional<TabAlertState> kAlertStatesToTest[] = {
      std::nullopt,
      TabAlertState::TAB_CAPTURING,
      TabAlertState::AUDIO_PLAYING,
      TabAlertState::AUDIO_MUTING,
      TabAlertState::PIP_PLAYING,
  };

  auto controller = std::make_unique<FakeTabSlotController>();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Tab* tab = widget->SetContentsView(std::make_unique<Tab>(controller.get()));

  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  TabRendererData data;
  data.favicon =
      ui::ImageModel::FromImageSkia(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

  // Perform layout over all possible combinations, checking for correct
  // results.
  for (bool is_pinned_tab : {false, true}) {
    for (bool is_active_tab : {false, true}) {
      for (std::optional<TabAlertState> alert_state : kAlertStatesToTest) {
        SCOPED_TRACE(
            ::testing::Message()
            << (is_active_tab ? "Active " : "Inactive ")
            << (is_pinned_tab ? "pinned " : "")
            << "tab with alert indicator state "
            << (alert_state ? static_cast<int>(alert_state.value()) : -1));

        data.pinned = is_pinned_tab;
        controller->set_active_tab(is_active_tab ? tab : nullptr);
        if (alert_state)
          data.alert_state = {alert_state.value()};
        else
          data.alert_state.clear();
        tab->SetData(data);
        StopFadeAnimationIfNecessary(*tab);

        // Test layout for every width from standard to minimum.
        int width, min_width;
        if (is_pinned_tab) {
          width = min_width = tab->tab_style()->GetPinnedWidth();
        } else {
          width = tab->tab_style()->GetStandardWidth();
          min_width = is_active_tab
                          ? TabStyle::Get()->GetMinimumActiveWidth()
                          : TabStyle::Get()->GetMinimumInactiveWidth();
        }
        const int height = GetLayoutConstant(TAB_HEIGHT);
        for (; width >= min_width; --width) {
          SCOPED_TRACE(::testing::Message() << "width=" << width);
          tab->SetBounds(0, 0, width, height);  // Invokes layout.
          CheckForExpectedLayoutAndVisibilityOfElements(*tab);
        }
      }
    }
  }
}

// Regression test for http://crbug.com/226253. Performing layout more than once
// shouldn't change the insets of the close button.
TEST_F(TabTest, CloseButtonLayout) {
  FakeTabSlotController tab_slot_controller;
  Tab tab(&tab_slot_controller);
  tab.SetBounds(0, 0, 100, 50);
  LayoutTab(&tab);
  gfx::Insets close_button_insets = GetCloseButton(&tab)->GetInsets();
  LayoutTab(&tab);
  gfx::Insets close_button_insets_2 = GetCloseButton(&tab)->GetInsets();
  EXPECT_EQ(close_button_insets.top(), close_button_insets_2.top());
  EXPECT_EQ(close_button_insets.left(), close_button_insets_2.left());
  EXPECT_EQ(close_button_insets.bottom(), close_button_insets_2.bottom());
  EXPECT_EQ(close_button_insets.right(), close_button_insets_2.right());
}

// Regression test for http://crbug.com/609701. Ensure TabCloseButton does not
// get focus on right click.
TEST_F(TabTest, CloseButtonFocus) {
  auto controller = std::make_unique<FakeTabSlotController>();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Tab* tab = widget->SetContentsView(std::make_unique<Tab>(controller.get()));

  TabCloseButton* tab_close_button = GetCloseButton(tab);

  // Verify tab_close_button does not get focus on right click.
  ui::MouseEvent right_click_event(ui::EventType::kKeyPressed, gfx::Point(),
                                   gfx::Point(), base::TimeTicks(),
                                   ui::EF_RIGHT_MOUSE_BUTTON, 0);
  tab_close_button->OnMousePressed(right_click_event);
  EXPECT_NE(tab_close_button,
            tab_close_button->GetFocusManager()->GetFocusedView());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(TabTest, CloseButtonHiddenWhenLockedForOnTask) {
  const auto tab_slot_controller = std::make_unique<FakeTabSlotController>();
  tab_slot_controller->SetLockedForOnTask(true);
  const std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  Tab* const tab =
      widget->SetContentsView(std::make_unique<Tab>(tab_slot_controller.get()));
  TabCloseButton* const tab_close_button = GetCloseButton(tab);
  EXPECT_FALSE(tab_close_button->GetVisible());
}

TEST_F(TabTest, CloseButtonShownWhenNotLockedForOnTask) {
  const auto tab_slot_controller = std::make_unique<FakeTabSlotController>();
  tab_slot_controller->SetLockedForOnTask(false);
  const std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  Tab* const tab =
      widget->SetContentsView(std::make_unique<Tab>(tab_slot_controller.get()));
  TabCloseButton* const tab_close_button = GetCloseButton(tab);
  EXPECT_TRUE(tab_close_button->GetVisible());
}
#endif

// Tests expected changes to the ThrobberView state when the WebContents loading
// state changes or the animation timer (usually in BrowserView) triggers.
TEST_F(TabTest, LayeredThrobber) {
  auto tab_slot_controller = std::make_unique<FakeTabSlotController>();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Tab* tab =
      widget->SetContentsView(std::make_unique<Tab>(tab_slot_controller.get()));
  tab->SizeToPreferredSize();

  TabIcon* icon = GetTabIcon(tab);
  SetupFakeClock(icon);
  TabRendererData data;
  data.visible_url = GURL("http://example.com");
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());
  EXPECT_EQ(TabNetworkState::kNone, tab->data().network_state);

  // Simulate a "normal" tab load: should paint to a layer.
  data.network_state = TabNetworkState::kWaiting;
  tab->SetData(data);
  EXPECT_TRUE(tab_slot_controller->CanPaintThrobberToLayer());
  EXPECT_TRUE(icon->GetShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  data.network_state = TabNetworkState::kLoading;
  tab->SetData(data);
  EXPECT_TRUE(icon->GetShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  data.network_state = TabNetworkState::kNone;
  tab->SetData(data);
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());

  // Simulate a tab that should hide throbber.
  data.should_hide_throbber = true;
  tab->SetData(data);
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());
  data.network_state = TabNetworkState::kWaiting;
  tab->SetData(data);
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());
  data.network_state = TabNetworkState::kLoading;
  tab->SetData(data);
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());
  data.network_state = TabNetworkState::kNone;
  tab->SetData(data);
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());

  // Simulate a tab that should not hide throbber.
  data.should_hide_throbber = false;
  data.network_state = TabNetworkState::kWaiting;
  tab->SetData(data);
  EXPECT_TRUE(tab_slot_controller->CanPaintThrobberToLayer());
  EXPECT_TRUE(icon->GetShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  data.network_state = TabNetworkState::kLoading;
  tab->SetData(data);
  EXPECT_TRUE(icon->GetShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  data.network_state = TabNetworkState::kNone;
  tab->SetData(data);
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());

  // After loading is done, simulate another resource starting to load.
  data.network_state = TabNetworkState::kWaiting;
  tab->SetData(data);
  EXPECT_TRUE(icon->GetShowingLoadingAnimation());

  // Reset.
  data.network_state = TabNetworkState::kNone;
  tab->SetData(data);
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());

  // Simulate a drag started and stopped during a load: layer painting stops
  // temporarily.
  data.network_state = TabNetworkState::kWaiting;
  tab->SetData(data);
  EXPECT_TRUE(icon->GetShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  tab_slot_controller->set_paint_throbber_to_layer(false);
  tab->StepLoadingAnimation(base::Milliseconds(100));
  EXPECT_TRUE(icon->GetShowingLoadingAnimation());
  EXPECT_FALSE(icon->layer());
  tab_slot_controller->set_paint_throbber_to_layer(true);
  tab->StepLoadingAnimation(base::Milliseconds(100));
  EXPECT_TRUE(icon->GetShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  data.network_state = TabNetworkState::kNone;
  tab->SetData(data);
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());

  // Simulate a tab load starting and stopping during tab dragging:
  // no layer painting.
  tab_slot_controller->set_paint_throbber_to_layer(false);
  data.network_state = TabNetworkState::kWaiting;
  tab->SetData(data);
  EXPECT_TRUE(icon->GetShowingLoadingAnimation());
  EXPECT_FALSE(icon->layer());
  data.network_state = TabNetworkState::kNone;
  tab->SetData(data);
  EXPECT_FALSE(icon->GetShowingLoadingAnimation());
}

TEST_F(TabTest, TitleHiddenWhenSmall) {
  FakeTabSlotController tab_slot_controller;
  Tab tab(&tab_slot_controller);
  tab.SetBounds(0, 0, 100, 50);
  EXPECT_GT(GetTitleWidth(&tab), 0);
  tab.SetBounds(0, 0, 0, 50);
  EXPECT_EQ(0, GetTitleWidth(&tab));
}

TEST_F(TabTest, FaviconDoesntMoveWhenShowingAlertIndicator) {
  auto controller = std::make_unique<FakeTabSlotController>();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  for (bool is_active_tab : {false, true}) {
    Tab* tab = widget->SetContentsView(std::make_unique<Tab>(controller.get()));
    controller->set_active_tab(is_active_tab ? tab : nullptr);
    tab->SizeToPreferredSize();

    views::View* icon = GetTabIcon(tab);
    int icon_x = icon->x();
    TabRendererData data;
    data.alert_state = {TabAlertState::AUDIO_PLAYING};
    tab->SetData(data);
    EXPECT_EQ(icon_x, icon->x());
  }
}

TEST_F(TabTest, SmallTabsHideCloseButton) {
  auto controller = std::make_unique<FakeTabSlotController>();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Tab* tab = widget->SetContentsView(std::make_unique<Tab>(controller.get()));
  const int width = tab->tab_style_views()->GetContentsInsets().width() +
                    Tab::kMinimumContentsWidthForCloseButtons;
  tab->SetBounds(0, 0, width, 50);
  const views::View* close = GetCloseButton(tab);
  EXPECT_TRUE(close->GetVisible());

  // Shrink the tab. The close button should disappear.
  tab->SetBounds(0, 0, width - 1, 50);
  EXPECT_FALSE(close->GetVisible());
}

TEST_F(TabTest, ExtraLeftPaddingShownOnSiteWithoutFavicon) {
  auto controller = std::make_unique<FakeTabSlotController>();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Tab* tab = widget->SetContentsView(std::make_unique<Tab>(controller.get()));

  tab->SizeToPreferredSize();
  const views::View* icon = GetTabIcon(tab);
  const int icon_x = icon->x() + icon->GetInsets().left();

  // Remove the favicon.
  TabRendererData data;
  data.show_icon = false;
  tab->SetData(data);
  EndTitleAnimation(tab);
  EXPECT_FALSE(icon->GetVisible());
  // Title should be placed where the favicon was.
  EXPECT_EQ(icon_x, GetTabTitle(tab)->x());
}

TEST_F(TabTest, ExtraAlertPaddingNotShownOnSmallActiveTab) {
  auto controller = std::make_unique<FakeTabSlotController>();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Tab* tab = widget->SetContentsView(std::make_unique<Tab>(controller.get()));
  controller->set_active_tab(tab);
  TabRendererData data;
  data.alert_state = {TabAlertState::AUDIO_PLAYING};
  tab->SetData(data);

  tab->SetBounds(0, 0, 200, 50);
  EXPECT_TRUE(GetTabIcon(tab)->GetVisible());
  const views::View* close = GetCloseButton(tab);
  const views::View* alert = GetAlertIndicator(tab);
  const int original_spacing = close->x() - alert->bounds().right();

  tab->SetBounds(0, 0, 90, 50);
  EXPECT_FALSE(GetTabIcon(tab)->GetVisible());

  tab->SetBounds(0, 0, 76, 50);
  EXPECT_TRUE(close->GetVisible());
  EXPECT_TRUE(alert->GetVisible());

  // The alert indicator moves closer because the extra padding is gone.
  EXPECT_LT(close->x() - alert->bounds().right(), original_spacing);

  tab->SetBounds(0, 0, 75, 50);
  EXPECT_TRUE(close->GetVisible());
  EXPECT_FALSE(alert->GetVisible());
}

TEST_F(TabTest, TitleTextHasSufficientContrast) {
  constexpr SkColor kDarkGray = SkColorSetRGB(0x22, 0x22, 0x22);
  constexpr SkColor kLightGray = SkColorSetRGB(0x99, 0x99, 0x99);
  struct ColorScheme {
    SkColor bg_active;
    SkColor fg_active;
    SkColor bg_inactive;
    SkColor fg_inactive;
  } color_schemes[] = {
      {
          SK_ColorBLACK, SK_ColorWHITE, SK_ColorBLACK, SK_ColorWHITE,
      },
      {
          SK_ColorBLACK, SK_ColorWHITE, SK_ColorWHITE, SK_ColorBLACK,
      },
      {
          kDarkGray, kLightGray, kDarkGray, kLightGray,
      },
  };

  auto controller = std::make_unique<FakeTabSlotController>();
  // Create a tab inside a Widget, so it has a theme provider, so the call to
  // UpdateForegroundColors() below doesn't no-op.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Tab* tab = widget->SetContentsView(std::make_unique<Tab>(controller.get()));

  for (const auto& colors : color_schemes) {
    tab->GetColorProvider()->SetColorForTesting(
        kColorTabBackgroundActiveFrameActive, colors.bg_active);
    tab->GetColorProvider()->SetColorForTesting(
        kColorTabBackgroundActiveFrameInactive, colors.bg_active);
    tab->GetColorProvider()->SetColorForTesting(
        kColorTabBackgroundInactiveFrameActive, colors.bg_inactive);
    tab->GetColorProvider()->SetColorForTesting(
        kColorTabBackgroundInactiveFrameInactive, colors.bg_inactive);
    controller->SetTabColors(colors.fg_active, colors.fg_inactive);
    for (TabActive active : {TabActive::kInactive, TabActive::kActive}) {
      controller->set_active_tab(active == TabActive::kActive ? tab : nullptr);
      tab->UpdateForegroundColors();
      const SkColor fg_color = tab->title_->GetEnabledColor();
      const SkColor bg_color = TabStyle::Get()->GetTabBackgroundColor(
          active == TabActive::kActive ? TabStyle::TabSelectionState::kActive
                                       : TabStyle::TabSelectionState::kInactive,
          /*hovered=*/false, tab->GetWidget()->ShouldPaintAsActive(),
          *tab->GetColorProvider());
      const float contrast = color_utils::GetContrastRatio(fg_color, bg_color);
      EXPECT_GE(contrast, color_utils::kMinimumReadableContrastRatio);
    }
  }
}

// This test verifies that the tab has its icon state updated when the alert
// animation fade-out finishes.
TEST_F(AlertIndicatorButtonTest, ShowsAndHidesAlertIndicator) {
  controller_->AddTab(0, TabActive::kInactive, TabPinned::kPinned);
  controller_->AddTab(1, TabActive::kActive);
  Tab* media_tab = tab_strip_->tab_at(0);

  // Pinned inactive tab only has an icon.
  EXPECT_TRUE(showing_icon(media_tab));
  EXPECT_FALSE(showing_alert_indicator(media_tab));
  EXPECT_FALSE(showing_close_button(media_tab));

  TabRendererData start_media;
  start_media.alert_state = {TabAlertState::AUDIO_PLAYING};
  start_media.pinned = media_tab->data().pinned;
  media_tab->SetData(std::move(start_media));

  // When audio starts, pinned inactive tab shows indicator.
  EXPECT_FALSE(showing_icon(media_tab));
  EXPECT_TRUE(showing_alert_indicator(media_tab));
  EXPECT_FALSE(showing_close_button(media_tab));

  TabRendererData stop_media;
  stop_media.pinned = media_tab->data().pinned;
  media_tab->SetData(std::move(stop_media));

  // When audio ends, pinned inactive tab fades out indicator.
  EXPECT_FALSE(showing_icon(media_tab));
  EXPECT_TRUE(showing_alert_indicator(media_tab));
  EXPECT_FALSE(showing_close_button(media_tab));

  // Rather than flakily waiting some unknown number of seconds for the fade
  // out animation to stop, reach out and stop the fade animation directly,
  // to make sure that it updates the tab appropriately when it's done.
  StopAnimation(media_tab);

  EXPECT_TRUE(showing_icon(media_tab));
  EXPECT_FALSE(showing_alert_indicator(media_tab));
  EXPECT_FALSE(showing_close_button(media_tab));
}

// This test verifies that the alert indicator for a camera and/or mic is
// visible at least for 5 seconds even if a camera/mic stopped being used.
TEST_F(AlertIndicatorButtonTest, MinHoldDurationTest) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      content_settings::features::kImprovedSemanticsActivityIndicators);

  controller_->AddTab(0, TabActive::kActive);
  Tab* media_tab = tab_strip_->tab_at(0);

  EXPECT_FALSE(showing_alert_indicator(media_tab));

  EXPECT_EQ(base::Time(), get_camera_mic_indicator_start_time(media_tab));

  TabRendererData start_media;
  start_media.alert_state = {TabAlertState::MEDIA_RECORDING};
  start_media.pinned = media_tab->data().pinned;
  media_tab->SetData(std::move(start_media));

  // When audio starts, pinned inactive tab shows indicator.
  EXPECT_TRUE(showing_alert_indicator(media_tab));
  EXPECT_NE(base::Time(), get_camera_mic_indicator_start_time(media_tab));

  TabRendererData stop_media;
  stop_media.pinned = media_tab->data().pinned;
  media_tab->SetData(std::move(stop_media));

  // The indicator's start time should be reset.
  EXPECT_EQ(base::Time(), get_camera_mic_indicator_start_time(media_tab));
  EXPECT_EQ(base::Seconds(5),
            get_fadeout_animation_duration_for_testing_(media_tab));
}

// This test verifies that the alert indicator for a camera and/or mic has
// 1-second fadeout animation after it was visible for longer than 5 seconds.
TEST_F(AlertIndicatorButtonTest, 1SecondFadeoutAnimationTest) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      content_settings::features::kImprovedSemanticsActivityIndicators);

  controller_->AddTab(0, TabActive::kActive);
  Tab* media_tab = tab_strip_->tab_at(0);

  EXPECT_FALSE(showing_alert_indicator(media_tab));

  EXPECT_EQ(base::Time(), get_camera_mic_indicator_start_time(media_tab));

  TabRendererData start_media;
  start_media.alert_state = {TabAlertState::MEDIA_RECORDING};
  start_media.pinned = media_tab->data().pinned;
  media_tab->SetData(std::move(start_media));

  // When audio starts, pinned inactive tab shows indicator.
  EXPECT_TRUE(showing_alert_indicator(media_tab));
  EXPECT_NE(base::Time(), get_camera_mic_indicator_start_time(media_tab));

  // After the indicator was displayed for 6 seconds, it should have 1-second
  // fadeout animation.
  task_environment()->AdvanceClock(base::Seconds(6));
  base::RunLoop().RunUntilIdle();

  TabRendererData stop_media;
  stop_media.pinned = media_tab->data().pinned;
  media_tab->SetData(std::move(stop_media));

  // The indicator's start time should be reset.
  EXPECT_EQ(base::Time(), get_camera_mic_indicator_start_time(media_tab));
  EXPECT_EQ(base::Seconds(1),
            get_fadeout_animation_duration_for_testing_(media_tab));
}

TEST_F(TabTest, DiscardIndicatorResponsiveness) {
  auto controller = std::make_unique<FakeTabSlotController>();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Tab* tab = widget->SetContentsView(std::make_unique<Tab>(controller.get()));
  const TabIcon* tab_icon = GetTabIcon(tab);

  struct TestCase {
    int tab_width;
    int expected_increased_radius;
  };
  std::list<TestCase> test_cases{
      {256, 2}, {45, 2}, {44, 2}, {43, 0}, {32, 0},
  };

  for (auto const& test_case : test_cases) {
    controller->SetInactiveTabWidth(test_case.tab_width);
    tab->SetBounds(0, 0, test_case.tab_width, 50);
    EXPECT_EQ(test_case.expected_increased_radius,
              tab_icon->increased_discard_indicator_radius_);
  }
}

TEST_F(TabTest, AccessibleProperties) {
  auto controller = std::make_unique<FakeTabSlotController>();
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Tab* tab = widget->SetContentsView(std::make_unique<Tab>(controller.get()));
  ui::AXNodeData data;

  tab->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kTab, data.role);
}
