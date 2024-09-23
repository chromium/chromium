// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_

#include <memory>
#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/view_observer.h"

class AlertIndicatorButton;
class TabCloseButton;
class TabSlotController;
class TabIcon;
struct TabSizeInfo;

namespace gfx {
class Animation;
class LinearAnimation;
}  // namespace gfx
namespace views {
class Label;
class View;
}  // namespace views

///////////////////////////////////////////////////////////////////////////////
//
//  A View that renders a Tab in a TabStrip.
//
///////////////////////////////////////////////////////////////////////////////
class Tab : public gfx::AnimationDelegate,
            public views::MaskedTargeterDelegate,
            public views::ViewObserver,
            public TabSlotView {
  METADATA_HEADER(Tab, TabSlotView)

 public:
  // When the content's width of the tab shrinks to below this size we should
  // hide the close button on inactive tabs. Any smaller and they're too easy
  // to hit on accident.
  static constexpr int kMinimumContentsWidthForCloseButtons = 68;
  static constexpr int kTouchMinimumContentsWidthForCloseButtons = 100;

  // Sets whether hover cards should appear on mouse hover. Used in browser
  // tests to prevent them from interfering with unrelated tests.
  static void SetShowHoverCardOnMouseHoverForTesting(bool value);

  explicit Tab(TabSlotController* controller);
  Tab(const Tab&) = delete;
  Tab& operator=(const Tab&) = delete;
  ~Tab() override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::MaskedTargeterDelegate:
  bool GetHitTestMask(SkPath* mask) const override;

  // TabSlotView:
  void Layout(PassKey) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void PaintChildren(const views::PaintInfo& info) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnFocus() override;
  void OnBlur() override;
  void OnThemeChanged() override;
  TabSlotView::ViewType GetTabSlotViewType() const override;
  TabSizeInfo GetTabSizeInfo() const override;

  TabSlotController* controller() const { return controller_; }

  // Used to set/check whether this Tab is being animated closed.
  void SetClosing(bool closing);
  bool closing() const { return closing_; }

  // Returns the color for the tab's group, if any.
  std::optional<SkColor> GetGroupColor() const;

  // Returns the color used for the alert indicator icon.
  ui::ColorId GetAlertIndicatorColor(TabAlertState state) const;

  // Returns true if this tab is the active tab.
  bool IsActive() const;

  // Notifies the AlertIndicatorButton that the active state of this tab has
  // changed.
  void ActiveStateChanged();

  // Called when the alert indicator has changed states.
  void AlertStateChanged();

  // Called when the selected state changes.
  void SelectedStateChanged();

  // Returns true if the tab is selected.
  bool IsSelected() const;

  // Returns true if this tab is discarded.
  bool IsDiscarded() const;

  // Returns true if this tab has captured a thumbnail.
  bool HasThumbnail() const;

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

  void CreateFreezingVote(content::WebContents* contents);
  void ReleaseFreezingVote();
  bool HasFreezingVote() const { return freezing_vote_.has_value(); }

  // Returns the width of the largest part of the tab that is available for the
  // user to click to select/activate the tab.
  int GetWidthOfLargestSelectableRegion() const;

  bool mouse_hovered() const { return mouse_hovered_; }

  // Returns the TabStyle associated with this tab.
  TabStyleViews* tab_style_views() { return tab_style_views_.get(); }
  const TabStyleViews* tab_style_views() const {
    return tab_style_views_.get();
  }
  const TabStyle* tab_style() const { return tab_style_views_->tab_style(); }

  // Returns the text to show in a tab's tooltip: The contents |title|, followed
  // by a break, followed by a localized string describing the |alert_state|.
  // Exposed publicly for tests.
  static std::u16string GetTooltipText(
      const std::u16string& title,
      std::optional<TabAlertState> alert_state);

  // Returns an alert state to be shown among given alert states.
  static std::optional<TabAlertState> GetAlertStateToShow(
      const std::vector<TabAlertState>& alert_states);

  bool showing_close_button_for_testing() const {
    return showing_close_button_;
  }

  raw_ptr<TabCloseButton> close_button() { return close_button_; }

  TabIcon* GetTabIconForTesting() const { return icon_; }

  AlertIndicatorButton* alert_indicator_button_for_testing() {
    return alert_indicator_button_;
  }

  void SetShouldShowDiscardIndicator(bool enabled);

 private:
  class TabCloseButtonObserver;
  friend class AlertIndicatorButtonTest;
  friend class TabTest;
  friend class TabStripTestBase;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  FRIEND_TEST_ALL_PREFIXES(TabStripTest, CloseButtonHiddenWhenLockedForOnTask);
#endif
  FRIEND_TEST_ALL_PREFIXES(TabStripTest, TabCloseButtonVisibility);
  FRIEND_TEST_ALL_PREFIXES(TabTest, TitleTextHasSufficientContrast);
  FRIEND_TEST_ALL_PREFIXES(TabHoverCardInteractiveUiTest,
                           HoverCardVisibleOnTabCloseButtonFocusAfterTabFocus);

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

  void CloseButtonPressed(const ui::Event& event);

  // The controller, never nullptr.
  const raw_ptr<TabSlotController> controller_;

  TabRendererData data_;

  std::unique_ptr<TabStyleViews> tab_style_views_;

  // True if the tab is being animated closed.
  bool closing_ = false;

  raw_ptr<TabIcon> icon_ = nullptr;
  raw_ptr<AlertIndicatorButton> alert_indicator_button_ = nullptr;
  raw_ptr<TabCloseButton> close_button_ = nullptr;

  raw_ptr<views::Label> title_;
  // The title's bounds are animated when switching between showing and hiding
  // the tab's favicon/throbber.
  gfx::Rect start_title_bounds_;
  gfx::Rect target_title_bounds_;
  gfx::LinearAnimation title_animation_;

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

  // Whether the tab is currently animating from a pinned to an unpinned state.
  bool is_animating_from_pinned_ = false;

  // When both the close button and alert indicator are visible, we add extra
  // padding between them to space them out visually.
  bool extra_alert_indicator_padding_ = false;

  // Indicates whether the mouse is currently hovered over the tab. This is
  // different from View::IsMouseHovered() which does a naive intersection with
  // the view bounds.
  bool mouse_hovered_ = false;

  std::unique_ptr<TabCloseButtonObserver> tab_close_button_observer_;

  // Freezing vote held while the tab is collapsed.
  std::optional<performance_manager::freezing::FreezingVote> freezing_vote_;

  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_H_
