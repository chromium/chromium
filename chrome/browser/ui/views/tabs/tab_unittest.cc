// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab.h"

#include <stddef.h>

#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_group_visual_data.h"
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/alert_indicator.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

using views::Widget;

class FakeTabController : public TabController {
 public:
  FakeTabController() {}

  void set_active_tab(bool value) { active_tab_ = value; }
  void set_paint_throbber_to_layer(bool value) {
    paint_throbber_to_layer_ = value;
  }

  const ui::ListSelectionModel& GetSelectionModel() const override {
    return selection_model_;
  }
  bool SupportsMultipleSelection() override { return false; }
  bool ShouldHideCloseButtonForTab(Tab* tab) const override { return false; }
  void SelectTab(Tab* tab, const ui::Event& event) override {}
  void ExtendSelectionTo(Tab* tab) override {}
  void ToggleSelected(Tab* tab) override {}
  void AddSelectionFromAnchorTo(Tab* tab) override {}
  void CloseTab(Tab* tab, CloseTabSource source) override {}
  void MoveTabRight(Tab* tab) override {}
  void MoveTabLeft(Tab* tab) override {}
  void MoveTabFirst(Tab* tab) override {}
  void MoveTabLast(Tab* tab) override {}
  void ShowContextMenuForTab(Tab* tab,
                             const gfx::Point& p,
                             ui::MenuSourceType source_type) override {}
  bool IsActiveTab(const Tab* tab) const override { return active_tab_; }
  bool IsTabSelected(const Tab* tab) const override { return false; }
  bool IsTabPinned(const Tab* tab) const override { return false; }
  bool IsFirstVisibleTab(const Tab* tab) const override { return false; }
  bool IsLastVisibleTab(const Tab* tab) const override { return false; }
  bool IsFocusInTabs() const override { return false; }
  void MaybeStartDrag(
      TabSlotView* source,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) override {}
  void ContinueDrag(views::View* view, const ui::LocatedEvent& event) override {
  }
  bool EndDrag(EndDragReason reason) override { return false; }
  Tab* GetTabAt(const gfx::Point& point) override { return nullptr; }
  const Tab* GetAdjacentTab(const Tab* tab, int offset) override {
    return nullptr;
  }
  void OnMouseEventInTab(views::View* source,
                         const ui::MouseEvent& event) override {}
  void UpdateHoverCard(Tab* tab) override {}
  bool HoverCardIsShowingForTab(Tab* tab) override { return false; }
  int GetBackgroundOffset() const override { return 0; }
  bool ShouldPaintAsActiveFrame() const override { return true; }
  int GetStrokeThickness() const override { return 0; }
  bool CanPaintThrobberToLayer() const override {
    return paint_throbber_to_layer_;
  }
  bool HasVisibleBackgroundTabShapes() const override { return false; }
  SkColor GetToolbarTopSeparatorColor() const override { return SK_ColorBLACK; }
  SkColor GetTabSeparatorColor() const override { return SK_ColorBLACK; }
  SkColor GetTabBackgroundColor(
      TabActive active,
      BrowserFrameActiveState active_state) const override {
    return active == TabActive::kActive ? tab_bg_color_active_
                                        : tab_bg_color_inactive_;
  }
  SkColor GetTabForegroundColor(TabActive active,
                                SkColor background_color) const override {
    return active == TabActive::kActive ? tab_fg_color_active_
                                        : tab_fg_color_inactive_;
  }
  base::Optional<int> GetCustomBackgroundId(
      BrowserFrameActiveState active_state) const override {
    return base::nullopt;
  }
  gfx::Rect GetTabAnimationTargetBounds(const Tab* tab) override {
    return tab->bounds();
  }
  base::string16 GetAccessibleTabName(const Tab* tab) const override {
    return base::string16();
  }
  float GetHoverOpacityForTab(float range_parameter) const override {
    return 1.0f;
  }
  float GetHoverOpacityForRadialHighlight() const override { return 1.0f; }

  const TabGroupVisualData* GetVisualDataForGroup(
      TabGroupId group) const override {
    return nullptr;
  }

  void SetVisualDataForGroup(TabGroupId group,
                             TabGroupVisualData visual_data) override {}

  void CloseAllTabsInGroup(TabGroupId group) override {}

  void UngroupAllTabsInGroup(TabGroupId group) override {}

  void AddNewTabInGroup(TabGroupId group) override {}

  const Browser* GetBrowser() override { return nullptr; }

  void SetTabColors(SkColor bg_color_active,
                    SkColor fg_color_active,
                    SkColor bg_color_inactive,
                    SkColor fg_color_inactive) {
    tab_bg_color_active_ = bg_color_active;
    tab_fg_color_active_ = fg_color_active;
    tab_bg_color_inactive_ = bg_color_inactive;
    tab_fg_color_inactive_ = fg_color_inactive;
  }

 private:
  ui::ListSelectionModel selection_model_;
  bool active_tab_ = false;
  bool paint_throbber_to_layer_ = true;

  SkColor tab_bg_color_active_ = gfx::kPlaceholderColor;
  SkColor tab_fg_color_active_ = gfx::kPlaceholderColor;
  SkColor tab_bg_color_inactive_ = gfx::kPlaceholderColor;
  SkColor tab_fg_color_inactive_ = gfx::kPlaceholderColor;

  DISALLOW_COPY_AND_ASSIGN(FakeTabController);
};

class TabTest : public ChromeViewsTestBase {
 public:
  TabTest() {
    // Prevent the fake clock from starting at 0 which is the null time.
    fake_clock_.Advance(base::TimeDelta::FromMilliseconds(2000));
  }
  ~TabTest() override {}

  static TabIcon* GetTabIcon(const Tab& tab) { return tab.icon_; }

  static views::Label* GetTabTitle(const Tab& tab) { return tab.title_; }

  static views::ImageView* GetAlertIndicator(const Tab& tab) {
    return tab.alert_indicator_;
  }

  static views::ImageButton* GetCloseButton(const Tab& tab) {
    return tab.close_button_;
  }

  static int GetTitleWidth(const Tab& tab) {
    return tab.title_->bounds().width();
  }

  static void EndTitleAnimation(Tab* tab) { tab->title_animation_.End(); }

  static void LayoutTab(Tab* tab) { tab->Layout(); }

  static int VisibleIconCount(const Tab& tab) {
    return tab.showing_icon_ + tab.showing_alert_indicator_ +
           tab.showing_close_button_;
  }

  static void CheckForExpectedLayoutAndVisibilityOfElements(const Tab& tab) {
    // Check whether elements are visible when they are supposed to be, given
    // Tab size and TabRendererData state.
    if (tab.data_.pinned) {
      EXPECT_EQ(1, VisibleIconCount(tab));
      if (tab.data_.alert_state) {
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
          if (tab.data_.alert_state) {
            EXPECT_FALSE(tab.showing_icon_);
            EXPECT_TRUE(tab.showing_alert_indicator_);
          } else {
            EXPECT_TRUE(tab.showing_icon_);
            EXPECT_FALSE(tab.showing_alert_indicator_);
          }
          break;
        default:
          EXPECT_EQ(3, VisibleIconCount(tab));
          EXPECT_TRUE(tab.data_.alert_state);
          break;
      }
    } else {  // Tab not active and not pinned tab.
      switch (VisibleIconCount(tab)) {
        case 1:
          EXPECT_FALSE(tab.showing_close_button_);
          if (!tab.data_.alert_state) {
            EXPECT_FALSE(tab.showing_alert_indicator_);
            EXPECT_TRUE(tab.showing_icon_);
          } else {
            EXPECT_FALSE(tab.showing_icon_);
            EXPECT_TRUE(tab.showing_alert_indicator_);
          }
          break;
        case 2:
          EXPECT_TRUE(tab.showing_icon_);
          if (tab.data_.alert_state)
            EXPECT_TRUE(tab.showing_alert_indicator_);
          else
            EXPECT_FALSE(tab.showing_alert_indicator_);
          break;
        default:
          EXPECT_EQ(3, VisibleIconCount(tab));
          EXPECT_TRUE(tab.data_.alert_state);
      }
    }

    // Check positioning of elements with respect to each other, and that they
    // are fully within the contents bounds.
    const gfx::Rect contents_bounds = tab.GetContentsBounds();
    if (tab.showing_icon_) {
      if (tab.center_icon_) {
        EXPECT_LE(tab.icon_->x(), contents_bounds.x());
      } else {
        EXPECT_LE(contents_bounds.x(), tab.icon_->x());
      }
      if (tab.title_->GetVisible())
        EXPECT_LE(tab.icon_->bounds().right(), tab.title_->x());
      EXPECT_LE(contents_bounds.y(), tab.icon_->y());
      EXPECT_LE(tab.icon_->bounds().bottom(), contents_bounds.bottom());
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
      EXPECT_LE(contents_bounds.y(), GetAlertIndicatorBounds(tab).y());
      EXPECT_LE(GetAlertIndicatorBounds(tab).bottom(),
                contents_bounds.bottom());
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
      // We need to use the close button contents bounds instead of its bounds,
      // since it has an empty border around it to extend its clickable area for
      // touch.
      // Note: The close button right edge can be outside the nominal contents
      // bounds, but shouldn't leave the local bounds.
      const gfx::Rect close_bounds = tab.close_button_->GetContentsBounds();
      EXPECT_LE(close_bounds.right(), tab.GetLocalBounds().right());
      EXPECT_LE(contents_bounds.y(), close_bounds.y());
      EXPECT_LE(close_bounds.bottom(), contents_bounds.bottom());
    }
  }

  static void StopFadeAnimationIfNecessary(const Tab& tab) {
    // Stop the fade animation directly instead of waiting an unknown number of
    // seconds.
    gfx::Animation* fade_animation =
        tab.alert_indicator_->fade_animation_.get();
    if (fade_animation)
      fade_animation->Stop();
  }

  void SetupFakeClock(TabIcon* icon) { icon->clock_ = &fake_clock_; }

 protected:
  void InitWidget(Widget* widget) {
    Widget::InitParams params(CreateParams(Widget::InitParams::TYPE_WINDOW));
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds.SetRect(10, 20, 300, 400);
    widget->Init(std::move(params));
  }

 private:
  static gfx::Rect GetAlertIndicatorBounds(const Tab& tab) {
    if (!tab.alert_indicator_) {
      ADD_FAILURE();
      return gfx::Rect();
    }
    return tab.alert_indicator_->bounds();
  }

  std::string original_locale_;
  base::SimpleTestTickClock fake_clock_;
};

class AlertIndicatorTest : public ChromeViewsTestBase {
 public:
  AlertIndicatorTest() {}
  ~AlertIndicatorTest() override {}

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    controller_ = new FakeBaseTabStripController;
    tab_strip_ = new TabStrip(std::unique_ptr<TabStripController>(controller_));
    controller_->set_tab_strip(tab_strip_);
    // The tab strip must be added to the view hierarchy for it to create the
    // buttons.
    parent_.AddChildView(tab_strip_);
    parent_.set_owned_by_client();

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams init_params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    init_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    init_params.bounds = gfx::Rect(0, 0, 400, 400);
    widget_->Init(std::move(init_params));
    widget_->SetContentsView(&parent_);
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

  void StopAnimation(Tab* tab) {
    ASSERT_TRUE(tab->alert_indicator_->fade_animation_);
    tab->alert_indicator_->fade_animation_->Stop();
  }

  // Owned by TabStrip.
  FakeBaseTabStripController* controller_ = nullptr;
  // Owns |tab_strip_|.
  views::View parent_;
  TabStrip* tab_strip_ = nullptr;
  std::unique_ptr<views::Widget> widget_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AlertIndicatorTest);
};

TEST_F(TabTest, HitTestTopPixel) {
  Widget widget;
  InitWidget(&widget);

  FakeTabController tab_controller;
  Tab tab(&tab_controller);
  widget.GetContentsView()->AddChildView(&tab);
  tab.SizeToPreferredSize();

  // Tabs are slanted, so a click halfway down the left edge won't hit it.
  int middle_y = tab.height() / 2;
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(0, middle_y)));

  // Tabs should not be hit if we click above them.
  int middle_x = tab.width() / 2;
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(middle_x, -1)));
  EXPECT_TRUE(tab.HitTestPoint(gfx::Point(middle_x, 0)));

  // Make sure top edge clicks still select the tab when the window is
  // maximized.
  widget.Maximize();
  EXPECT_TRUE(tab.HitTestPoint(gfx::Point(middle_x, 0)));

  // But clicks in the area above the slanted sides should still miss.
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(0, 0)));
  EXPECT_FALSE(tab.HitTestPoint(gfx::Point(tab.width() - 1, 0)));
}

TEST_F(TabTest, LayoutAndVisibilityOfElements) {
  static const base::Optional<TabAlertState> kAlertStatesToTest[] = {
      base::nullopt,
      TabAlertState::TAB_CAPTURING,
      TabAlertState::AUDIO_PLAYING,
      TabAlertState::AUDIO_MUTING,
      TabAlertState::PIP_PLAYING,
  };

  Widget widget;
  InitWidget(&widget);

  FakeTabController controller;
  Tab tab(&controller);
  widget.GetContentsView()->AddChildView(&tab);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  TabRendererData data;
  data.favicon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  // Perform layout over all possible combinations, checking for correct
  // results.
  for (bool is_pinned_tab : {false, true}) {
    for (bool is_active_tab : {false, true}) {
      for (base::Optional<TabAlertState> alert_state : kAlertStatesToTest) {
        SCOPED_TRACE(
            ::testing::Message()
            << (is_active_tab ? "Active " : "Inactive ")
            << (is_pinned_tab ? "pinned " : "")
            << "tab with alert indicator state "
            << (alert_state ? static_cast<int>(alert_state.value()) : -1));

        data.pinned = is_pinned_tab;
        controller.set_active_tab(is_active_tab);
        data.alert_state = alert_state;
        tab.SetData(data);
        StopFadeAnimationIfNecessary(tab);

        // Test layout for every width from standard to minimum.
        int width, min_width;
        if (is_pinned_tab) {
          width = min_width = TabStyle::GetPinnedWidth();
        } else {
          width = TabStyle::GetStandardWidth();
          min_width = is_active_tab ? TabStyleViews::GetMinimumActiveWidth()
                                    : TabStyleViews::GetMinimumInactiveWidth();
        }
        const int height = GetLayoutConstant(TAB_HEIGHT);
        for (; width >= min_width; --width) {
          SCOPED_TRACE(::testing::Message() << "width=" << width);
          tab.SetBounds(0, 0, width, height);  // Invokes Tab::Layout().
          CheckForExpectedLayoutAndVisibilityOfElements(tab);
        }
      }
    }
  }
}

// Regression test for http://crbug.com/420313: Confirms that any child Views of
// Tab do not attempt to provide their own tooltip behavior/text.
TEST_F(TabTest, TooltipProvidedByTab) {
  // This test isn't relevant when tab hover cards are enabled since tab
  // tooltips are then disabled.
  if (base::FeatureList::IsEnabled(features::kTabHoverCards))
    return;
  Widget widget;
  InitWidget(&widget);

  FakeTabController controller;
  Tab tab(&controller);
  widget.GetContentsView()->AddChildView(&tab);
  tab.SizeToPreferredSize();

  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  TabRendererData data;
  data.favicon = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  data.title = base::UTF8ToUTF16(
      "This is a really long tab title that would cause views::Label to "
      "provide its own tooltip; but Tab should disable that feature so it can "
      "provide the tooltip instead.");

  // Test both with and without an indicator showing since the tab tooltip text
  // should include a description of the alert state when the indicator is
  // present.
  for (int i = 0; i < 2; ++i) {
    data.alert_state = (i == 0 ? base::Optional<TabAlertState>()
                               : TabAlertState::AUDIO_PLAYING);
    SCOPED_TRACE(::testing::Message()
                 << "Tab with alert indicator state "
                 << (data.alert_state
                         ? static_cast<int>(data.alert_state.value())
                         : -1));
    tab.SetData(data);
    const base::string16 expected_tooltip =
        Tab::GetTooltipText(data.title, data.alert_state);

    for (auto j = tab.children().begin(); j != tab.children().end(); ++j) {
      if (!strcmp((*j)->GetClassName(), "TabCloseButton"))
        continue;  // Close button is excepted.
      if (!(*j)->GetVisible())
        continue;
      SCOPED_TRACE(::testing::Message()
                   << "child " << std::distance(tab.children().begin(), j)
                   << ": " << (*j)->GetClassName());

      const gfx::Point midpoint((*j)->width() / 2, (*j)->height() / 2);
      EXPECT_FALSE((*j)->GetTooltipHandlerForPoint(midpoint));
      const gfx::Point mouse_hover_point =
          midpoint + (*j)->GetMirroredPosition().OffsetFromOrigin();
      EXPECT_EQ(expected_tooltip, tab.GetTooltipText(mouse_hover_point));
    }
  }
}

// Regression test for http://crbug.com/226253. Calling Layout() more than once
// shouldn't change the insets of the close button.
TEST_F(TabTest, CloseButtonLayout) {
  FakeTabController tab_controller;
  Tab tab(&tab_controller);
  tab.SetBounds(0, 0, 100, 50);
  LayoutTab(&tab);
  gfx::Insets close_button_insets = GetCloseButton(tab)->GetInsets();
  LayoutTab(&tab);
  gfx::Insets close_button_insets_2 = GetCloseButton(tab)->GetInsets();
  EXPECT_EQ(close_button_insets.top(), close_button_insets_2.top());
  EXPECT_EQ(close_button_insets.left(), close_button_insets_2.left());
  EXPECT_EQ(close_button_insets.bottom(), close_button_insets_2.bottom());
  EXPECT_EQ(close_button_insets.right(), close_button_insets_2.right());

  // Also make sure the close button is sized as large as the tab.
  EXPECT_EQ(50, GetCloseButton(tab)->bounds().height());
}

// Regression test for http://crbug.com/609701. Ensure TabCloseButton does not
// get focus on right click.
TEST_F(TabTest, CloseButtonFocus) {
  Widget widget;
  InitWidget(&widget);
  FakeTabController tab_controller;
  Tab tab(&tab_controller);
  widget.GetContentsView()->AddChildView(&tab);

  views::ImageButton* tab_close_button = GetCloseButton(tab);

  // Verify tab_close_button does not get focus on right click.
  ui::MouseEvent right_click_event(ui::ET_KEY_PRESSED, gfx::Point(),
                                   gfx::Point(), base::TimeTicks(),
                                   ui::EF_RIGHT_MOUSE_BUTTON, 0);
  tab_close_button->OnMousePressed(right_click_event);
  EXPECT_NE(tab_close_button,
            tab_close_button->GetFocusManager()->GetFocusedView());
}

// Tests expected changes to the ThrobberView state when the WebContents loading
// state changes or the animation timer (usually in BrowserView) triggers.
TEST_F(TabTest, LayeredThrobber) {
  Widget widget;
  InitWidget(&widget);

  FakeTabController tab_controller;
  Tab tab(&tab_controller);
  widget.GetContentsView()->AddChildView(&tab);
  tab.SizeToPreferredSize();

  TabIcon* icon = GetTabIcon(tab);
  SetupFakeClock(icon);
  TabRendererData data;
  data.visible_url = GURL("http://example.com");
  EXPECT_FALSE(icon->ShowingLoadingAnimation());
  EXPECT_EQ(TabNetworkState::kNone, tab.data().network_state);

  // Simulate a "normal" tab load: should paint to a layer.
  data.network_state = TabNetworkState::kWaiting;
  tab.SetData(data);
  EXPECT_TRUE(tab_controller.CanPaintThrobberToLayer());
  EXPECT_TRUE(icon->ShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  data.network_state = TabNetworkState::kLoading;
  tab.SetData(data);
  EXPECT_TRUE(icon->ShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  data.network_state = TabNetworkState::kNone;
  tab.SetData(data);
  EXPECT_FALSE(icon->ShowingLoadingAnimation());

  // Simulate a tab that should hide throbber.
  data.should_hide_throbber = true;
  tab.SetData(data);
  EXPECT_FALSE(icon->ShowingLoadingAnimation());
  data.network_state = TabNetworkState::kWaiting;
  tab.SetData(data);
  EXPECT_FALSE(icon->ShowingLoadingAnimation());
  data.network_state = TabNetworkState::kLoading;
  tab.SetData(data);
  EXPECT_FALSE(icon->ShowingLoadingAnimation());
  data.network_state = TabNetworkState::kNone;
  tab.SetData(data);
  EXPECT_FALSE(icon->ShowingLoadingAnimation());

  // Simulate a tab that should not hide throbber.
  data.should_hide_throbber = false;
  data.network_state = TabNetworkState::kWaiting;
  tab.SetData(data);
  EXPECT_TRUE(tab_controller.CanPaintThrobberToLayer());
  EXPECT_TRUE(icon->ShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  data.network_state = TabNetworkState::kLoading;
  tab.SetData(data);
  EXPECT_TRUE(icon->ShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  data.network_state = TabNetworkState::kNone;
  tab.SetData(data);
  EXPECT_FALSE(icon->ShowingLoadingAnimation());

  // After loading is done, simulate another resource starting to load.
  data.network_state = TabNetworkState::kWaiting;
  tab.SetData(data);
  EXPECT_TRUE(icon->ShowingLoadingAnimation());

  // Reset.
  data.network_state = TabNetworkState::kNone;
  tab.SetData(data);
  EXPECT_FALSE(icon->ShowingLoadingAnimation());

  // Simulate a drag started and stopped during a load: layer painting stops
  // temporarily.
  data.network_state = TabNetworkState::kWaiting;
  tab.SetData(data);
  EXPECT_TRUE(icon->ShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  tab_controller.set_paint_throbber_to_layer(false);
  tab.StepLoadingAnimation(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(icon->ShowingLoadingAnimation());
  EXPECT_FALSE(icon->layer());
  tab_controller.set_paint_throbber_to_layer(true);
  tab.StepLoadingAnimation(base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(icon->ShowingLoadingAnimation());
  EXPECT_TRUE(icon->layer());
  data.network_state = TabNetworkState::kNone;
  tab.SetData(data);
  EXPECT_FALSE(icon->ShowingLoadingAnimation());

  // Simulate a tab load starting and stopping during tab dragging (or with
  // stacked tabs): no layer painting.
  tab_controller.set_paint_throbber_to_layer(false);
  data.network_state = TabNetworkState::kWaiting;
  tab.SetData(data);
  EXPECT_TRUE(icon->ShowingLoadingAnimation());
  EXPECT_FALSE(icon->layer());
  data.network_state = TabNetworkState::kNone;
  tab.SetData(data);
  EXPECT_FALSE(icon->ShowingLoadingAnimation());
}

TEST_F(TabTest, TitleHiddenWhenSmall) {
  FakeTabController tab_controller;
  Tab tab(&tab_controller);
  tab.SetBounds(0, 0, 100, 50);
  EXPECT_GT(GetTitleWidth(tab), 0);
  tab.SetBounds(0, 0, 0, 50);
  EXPECT_EQ(0, GetTitleWidth(tab));
}

TEST_F(TabTest, FaviconDoesntMoveWhenShowingAlertIndicator) {
  Widget widget;
  InitWidget(&widget);

  for (bool is_active_tab : {false, true}) {
    FakeTabController controller;
    controller.set_active_tab(is_active_tab);
    Tab tab(&controller);
    widget.GetContentsView()->AddChildView(&tab);
    tab.SizeToPreferredSize();

    views::View* icon = GetTabIcon(tab);
    int icon_x = icon->x();
    TabRendererData data;
    data.alert_state = TabAlertState::AUDIO_PLAYING;
    tab.SetData(data);
    EXPECT_EQ(icon_x, icon->x());
  }
}

TEST_F(TabTest, SmallTabsHideCloseButton) {
  Widget widget;
  InitWidget(&widget);

  FakeTabController controller;
  controller.set_active_tab(false);
  Tab tab(&controller);
  widget.GetContentsView()->AddChildView(&tab);
  const int width = tab.tab_style()->GetContentsInsets().width() +
                    Tab::kMinimumContentsWidthForCloseButtons;
  tab.SetBounds(0, 0, width, 50);
  const views::View* close = GetCloseButton(tab);
  EXPECT_TRUE(close->GetVisible());

  const views::View* icon = GetTabIcon(tab);
  const int icon_x = icon->x();
  // Shrink the tab. The close button should disappear.
  tab.SetBounds(0, 0, width - 1, 50);
  EXPECT_FALSE(close->GetVisible());
  // The favicon moves left because the extra padding disappears too.
  EXPECT_LT(icon->x(), icon_x);
}

TEST_F(TabTest, ExtraLeftPaddingNotShownOnSmallActiveTab) {
  Widget widget;
  InitWidget(&widget);

  FakeTabController controller;
  controller.set_active_tab(true);
  Tab tab(&controller);
  widget.GetContentsView()->AddChildView(&tab);
  tab.SetBounds(0, 0, 200, 50);
  const views::View* close = GetCloseButton(tab);
  EXPECT_TRUE(close->GetVisible());

  const views::View* icon = GetTabIcon(tab);
  const int icon_x = icon->x();

  tab.SetBounds(0, 0, 40, 50);
  EXPECT_TRUE(close->GetVisible());
  // The favicon moves left because the extra padding disappears.
  EXPECT_LT(icon->x(), icon_x);
}

TEST_F(TabTest, ExtraLeftPaddingShownOnSiteWithoutFavicon) {
  Widget widget;
  InitWidget(&widget);

  FakeTabController controller;
  Tab tab(&controller);
  widget.GetContentsView()->AddChildView(&tab);

  tab.SizeToPreferredSize();
  const views::View* icon = GetTabIcon(tab);
  const int icon_x = icon->x();

  // Remove the favicon.
  TabRendererData data;
  data.show_icon = false;
  tab.SetData(data);
  EndTitleAnimation(&tab);
  EXPECT_FALSE(icon->GetVisible());
  // Title should be placed where the favicon was.
  EXPECT_EQ(icon_x, GetTabTitle(tab)->x());
}

TEST_F(TabTest, ExtraAlertPaddingNotShownOnSmallActiveTab) {
  Widget widget;
  InitWidget(&widget);

  FakeTabController controller;
  controller.set_active_tab(true);
  Tab tab(&controller);
  widget.GetContentsView()->AddChildView(&tab);
  TabRendererData data;
  data.alert_state = TabAlertState::AUDIO_PLAYING;
  tab.SetData(data);

  tab.SetBounds(0, 0, 200, 50);
  EXPECT_TRUE(GetTabIcon(tab)->GetVisible());
  const views::View* close = GetCloseButton(tab);
  const views::View* alert = GetAlertIndicator(tab);
  const int original_spacing = close->x() - alert->bounds().right();

  tab.SetBounds(0, 0, 70, 50);
  EXPECT_FALSE(GetTabIcon(tab)->GetVisible());
  EXPECT_TRUE(close->GetVisible());
  EXPECT_TRUE(alert->GetVisible());
  // The alert indicator moves closer because the extra padding is gone.
  EXPECT_LT(close->x() - alert->bounds().right(), original_spacing);
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

  // Create a tab inside a Widget, so it has a theme provider, so the call to
  // UpdateForegroundColors() below doesn't no-op.
  Widget widget;
  InitWidget(&widget);
  FakeTabController controller;
  Tab tab(&controller);
  widget.GetContentsView()->AddChildView(&tab);

  for (const auto& colors : color_schemes) {
    controller.SetTabColors(colors.bg_active, colors.fg_active,
                            colors.bg_inactive, colors.fg_inactive);
    for (TabActive active : {TabActive::kInactive, TabActive::kActive}) {
      controller.set_active_tab(active == TabActive::kActive);
      tab.UpdateForegroundColors();
      const SkColor fg_color = tab.title_->GetEnabledColor();
      const SkColor bg_color = controller.GetTabBackgroundColor(
          active, BrowserFrameActiveState::kUseCurrent);
      const float contrast = color_utils::GetContrastRatio(fg_color, bg_color);
      EXPECT_GE(contrast, color_utils::kMinimumReadableContrastRatio);
    }
  }
}

// This test verifies that the tab has its icon state updated when the alert
// animation fade-out finishes.
TEST_F(AlertIndicatorTest, ShowsAndHidesAlertIndicator) {
  controller_->AddPinnedTab(0, false);
  controller_->AddTab(1, true);
  Tab* media_tab = tab_strip_->tab_at(0);

  // Pinned inactive tab only has an icon.
  EXPECT_TRUE(showing_icon(media_tab));
  EXPECT_FALSE(showing_alert_indicator(media_tab));
  EXPECT_FALSE(showing_close_button(media_tab));

  TabRendererData start_media;
  start_media.alert_state = TabAlertState::AUDIO_PLAYING;
  start_media.pinned = media_tab->data().pinned;
  media_tab->SetData(std::move(start_media));

  // When audio starts, pinned inactive tab shows indicator.
  EXPECT_FALSE(showing_icon(media_tab));
  EXPECT_TRUE(showing_alert_indicator(media_tab));
  EXPECT_FALSE(showing_close_button(media_tab));

  TabRendererData stop_media;
  stop_media.alert_state = base::nullopt;
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
