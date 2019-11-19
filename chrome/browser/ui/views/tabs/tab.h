// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_

#include <list>
#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "ui/base/layout.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/view_observer.h"

class AlertIndicator;
class TabCloseButton;
class TabController;
class TabIcon;
struct TabSizeInfo;
class TabStyleViews;

namespace gfx {
class Animation;
class LinearAnimation;
}  // namespace gfx
namespace views {
class Label;
class View;
}

///////////////////////////////////////////////////////////////////////////////
//
//  A View that renders a Tab in a TabStrip.
//
///////////////////////////////////////////////////////////////////////////////
class Tab : public gfx::AnimationDelegate,
            public views::ButtonListener,
            public views::MaskedTargeterDelegate,
            public views::ViewObserver,
            public TabSlotView {
 public:
  // The Tab's class name.
  static const char kViewClassName[];

  // When the content's width of the tab shrinks to below this size we should
  // hide the close button on inactive tabs. Any smaller and they're too easy
  // to hit on accident.
  static constexpr int kMinimumContentsWidthForCloseButtons = 68;
  static constexpr int kTouchMinimumContentsWidthForCloseButtons = 100;

  explicit Tab(TabController* controller);
  ~Tab() override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::MaskedTargeterDelegate:
  bool GetHitTestMask(SkPath* mask) const override;

  // TabSlotView:
  void Layout() override;
  const char* GetClassName() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
  void PaintChildren(const views::PaintInfo& info) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void AddedToWidget() override;
  void OnFocus() override;
  void OnBlur() override;
  void OnThemeChanged() override;
  TabSlotView::ViewType GetTabSlotViewType() const override;
  TabSizeInfo GetTabSizeInfo() const override;

  TabController* controller() const { return controller_; }

  // Used to set/check whether this Tab is being animated closed.
  void SetClosing(bool closing);
  bool closing() const { return closing_; }

  // Returns the color for the tab's group, if any.
  base::Optional<SkColor> GetGroupColor() const;

  // Returns the color used for the alert indicator icon.
  SkColor GetAlertIndicatorColor(TabAlertState state) const;

  // Returns true if this tab is the active tab.
  bool IsActive() const;

  // Notifies the AlertIndicatorButton that the active state of this tab has
  // changed.
  void ActiveStateChanged();

  // Called when the alert indicator has changed states.
  void AlertStateChanged();

  // Called when the frame state color changes.
  void FrameColorsChanged();

  // Called when the selected state changes.
  void SelectedStateChanged();

  // Returns true if the tab is selected.
  bool IsSelected() const;

  // Sets the data this tabs displays. Should only be called after Tab is added
  // to widget hierarchy.
  void SetData(TabRendererData data);
  const TabRendererData& data() const { return data_; }

  // Redraws the loading animation if one is visible. Otherwise, no-op. The
  // |elapsed_time| parameter is shared between tabs and used to keep the
  // throbbers in sync.
  void StepLoadingAnimation(const base::TimeDelta& elapsed_time);

  // Sets the visibility of the indicator shown when the tab needs to indicate
  // to the user that it needs their attention.
  void SetTabNeedsAttention(bool attention);

  // Returns true if this tab became the active tab selected in
  // response to the last ui::ET_TAP_DOWN gesture dispatched to
  // this tab. Only used for collecting UMA metrics.
  // See ash/touch/touch_uma.cc.
  bool tab_activated_with_last_tap_down() const {
    return tab_activated_with_last_tap_down_;
  }

  bool mouse_hovered() const { return mouse_hovered_; }

  // Returns the TabStyle associated with this tab.
  TabStyleViews* tab_style() { return tab_style_.get(); }
  const TabStyleViews* tab_style() const { return tab_style_.get(); }

  // Returns the text to show in a tab's tooltip: The contents |title|, followed
  // by a break, followed by a localized string describing the |alert_state|.
  // Exposed publicly for tests.
  static base::string16 GetTooltipText(
      const base::string16& title,
      base::Optional<TabAlertState> alert_state);

 private:
  class TabCloseButtonObserver;
  friend class AlertIndicatorTest;
  friend class TabTest;
  friend class TabStripTest;
  FRIEND_TEST_ALL_PREFIXES(TabStripTest, TabCloseButtonVisibilityWhenStacked);
  FRIEND_TEST_ALL_PREFIXES(TabStripTest,
                           TabCloseButtonVisibilityWhenNotStacked);
  FRIEND_TEST_ALL_PREFIXES(TabTest, TitleTextHasSufficientContrast);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardBubbleViewBrowserTest,
                           WidgetVisibleOnTabCloseButtonFocusAfterTabFocus);

  // Invoked from Layout to adjust the position of the favicon or alert
  // indicator for pinned tabs. The visual_width parameter is how wide the
  // icon looks (rather than how wide the bounds are).
  void MaybeAdjustLeftForPinnedTab(gfx::Rect* bounds, int visual_width) const;

  // Computes which icons are visible in the tab. Should be called everytime
  // before layout is performed.
  void UpdateIconVisibility();

  // Returns whether the tab should be rendered as a normal tab as opposed to a
  // pinned tab.
  bool ShouldRenderAsNormalTab() const;

  // Updates the blocked attention state of the |icon_|. This only updates
  // state; it is the responsibility of the caller to request a paint.
  void UpdateTabIconNeedsAttentionBlocked();

  // Selects, generates, and applies colors for various foreground elements to
  // ensure proper contrast. Elements affected include title text, close button
  // and alert icon.
  void UpdateForegroundColors();

  // Considers switching to hovered mode or [re-]showing the hover card based on
  // the mouse moving over the tab. If the tab is already hovered or mouse
  // events are disabled because of touch input, this is a no-op.
  void MaybeUpdateHoverStatus(const ui::MouseEvent& event);

  // The controller, never nullptr.
  TabController* const controller_;

  TabRendererData data_;

  std::unique_ptr<TabStyleViews> tab_style_;

  // True if the tab is being animated closed.
  bool closing_ = false;

  TabIcon* icon_ = nullptr;
  AlertIndicator* alert_indicator_ = nullptr;
  TabCloseButton* close_button_ = nullptr;

  views::Label* title_;
  // The title's bounds are animated when switching between showing and hiding
  // the tab's favicon/throbber.
  gfx::Rect start_title_bounds_;
  gfx::Rect target_title_bounds_;
  gfx::LinearAnimation title_animation_;

  bool tab_activated_with_last_tap_down_ = false;

  // For narrow tabs, we show the alert icon or, if there is no alert icon, the
  // favicon even if it won't completely fit. In this case, we need to center
  // the icon within the tab; it will be clipped to fit.
  bool center_icon_ = false;

  // Whether we're showing the icon. It is cached so that we can detect when it
  // changes and layout appropriately.
  bool showing_icon_ = false;

  // Whether we're showing the alert indicator. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_alert_indicator_ = false;

  // Whether we are showing the close button. It is cached so that we can
  // detect when it changes and layout appropriately.
  bool showing_close_button_ = false;

  // If there's room, we add additional padding to the left of the favicon to
  // balance the whitespace inside the non-hovered close button image;
  // otherwise, the tab contents look too close to the left edge. Once the tabs
  // get too small, we let the tab contents take the full width, to maximize
  // visible area.
  bool extra_padding_before_content_ = false;

  // When both the close button and alert indicator are visible, we add extra
  // padding between them to space them out visually.
  bool extra_alert_indicator_padding_ = false;

  // The tab foreground color (title, buttons).
  SkColor foreground_color_ = SK_ColorTRANSPARENT;

  // Indicates whether the mouse is currently hovered over the tab. This is
  // different from View::IsMouseHovered() which does a naive intersection with
  // the view bounds.
  bool mouse_hovered_ = false;

  std::unique_ptr<TabCloseButtonObserver> tab_close_button_observer_;

  // Focus ring for accessibility.
  std::unique_ptr<views::FocusRing> focus_ring_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_
