// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_VIEW_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/hovercard/hover_card_anchor_target.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/tab/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"
#include "chrome/common/buildflags.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class GlowHoverController;
class TabCloseButton;
class TabCollectionNode;
class TabIcon;
class TabTitle;
class TabStyleViews;

namespace base {
class TimeDelta;
}  // namespace base

namespace glic {
class TabUnderlineView;
}  // namespace glic

// The view class for the tab. It is responsible for painting the
// tab background and displaying the favicon, title, alert indicators and close
// button. It handles data changed event and view hierarchy changes to updates
// its states. The tab view implements its own layout and avoids using
// FlexLayout for performance reasons.
class TabView : public views::View,
                public views::MaskedTargeterDelegate,
                public AlertIndicatorButton::Delegate,
                public views::ContextMenuController,
                public HoverCardAnchorTarget,
                public views::ViewObserver {
  METADATA_HEADER(TabView, views::View)
  friend class TabStyleViewDelegateImpl;

 public:
  static constexpr base::TimeDelta kGlowHoverAnimationDuration =
      base::Milliseconds(50);

  explicit TabView(TabCollectionNode* collection_node);
  TabView(const TabView&) = delete;
  TabView& operator=(const TabView&) = delete;
  ~TabView() override;

  class LayoutManager : public views::LayoutManagerBase {
   public:
    virtual void OnTabClosing() {}

   protected:
    // views::LayoutManagerBase:
    void OnInstalled(views::View* host) override;

    // Casts host_view() to a TabView const ref, using static_cast. Avoids
    // views::AsViewClass as it incurs overhead when checking metadata.
    const TabView& TabView() const;
  };

  void StepLoadingAnimation(const base::TimeDelta& elapsed_time);

  void CreateFreezingVote(FreezingVoteReason reason);
  void ReleaseFreezingVote(FreezingVoteReason reason);
  bool HasFreezingVote(FreezingVoteReason reason) const;
  bool HasFreezingVote() const;
  void UpdateFocusFreezing();

  void UpdateHovered(bool hovered);
  bool IsHoverAnimationActive() const;

  std::optional<SkColor> GetBackgroundColor();
  SkPath GetPath() const;

  const TabCollectionNode* collection_node() const { return collection_node_; }
  TabStyleViews* tab_styling() { return tab_styling_.get(); }
  const TabStyleViews* tab_styling() const { return tab_styling_.get(); }
  float radial_highlight_opacity() { return radial_highlight_opacity_; }
  const tabs::TabData& data() const { return tab_data_; }
  bool IsActive() const { return active_; }
  bool IsClosing() const { return !collection_node_; }
  bool split() const { return split_; }
  bool pinned() const { return pinned_; }
  const tabs::TabInterface* GetTabInterface() const;

  GlowHoverController* GetHoverControllerForTesting() {
    return hover_controller_.get();
  }

  TabCloseButton* close_button_for_testing() { return close_button_; }
  TabIcon* GetTabIconForTesting() { return icon_; }
  void SetDataForTesting(tabs::TabData data);

  // HoverCardAnchorTarget:
  bool NeedsToShowThumbnail() const override;
  bool IsValidHoverCardTarget() const override;
  views::BubbleAnchor GetAnchor() override;
  views::BubbleBorder::Arrow GetAnchorPosition() const override;

 private:
  friend class TabViewVerticalLayout;
  friend class TabViewHorizontalLayout;

  // views::View
  gfx::Size GetMinimumSize() const override;
  void Layout(PassKey) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void PaintChildren(const views::PaintInfo& info) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnFocus() override;
  void OnBlur() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnThemeChanged() override;
  void UpdateParentLayer() override;

  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

  // views::MaskedTargeterDelegate:
  bool GetHitTestMask(SkPath* mask) const override;

  // AlertIndicatorButton::Delegate
  bool ShouldEnableMuteToggle(int required_width) override;
  void ToggleTabAudioMute() override;
  bool IsApparentlyActive() const override;
  void AlertStateChanged() override;

  // ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override;

  void ResetCollectionNode();

  void UpdateAccessibleName();
  void OnFrameActiveStateChanged();
  void OnAXNameChanged(ax::mojom::StringAttribute attribute,
                       const std::optional<std::string>& name);
  void OnCollapseStateChanged(tabs::VerticalTabStripCollapseState state);
  void OnTabStateChanged();
  void OnTabDataChanged(TabChangeType change_type, const tabs::TabData& data);
  void SetSelection(bool selected);
  void UpdateTabData(const tabs::TabInterface* tab);

  void UpdateTitle(std::u16string title, bool should_render_loading_title);
  void UpdateBorder();
  void UpdateColors();
  void UpdateContrastRatioValues();

  void CloseButtonPressed(const ui::Event& event);
  void RecordMousePressedInTab();

  void UpdateHoverCard(HoverCardAnchorTarget* target,
                       int hover_card_update_type);

  double GetHoverAnimationValue() const;
  float GetHoverOpacity() const;

  bool IsFrameActive() const;
  TabStyle::TabSelectionState GetSelectionState() const;

  bool IsDragging() const;

  static int UncollapsedMinWidth();
  static int CollapsedWidth();

  bool IsInExpandOnHover(int width) const;

  SkScalar GetCornerRadius() const;

  // Applies rounded corners to the view's layer.
  void UpdateLayerRoundedCorners();

  void UpdateZOrder();

  raw_ptr<TabCollectionNode> collection_node_ = nullptr;
  TabStripOrientation orientation_ = TabStripOrientation::kHorizontal;

  std::unique_ptr<TabStyleViews> tab_styling_;

  const raw_ptr<TabIcon> icon_;
  const raw_ptr<TabTitle> title_;
  const raw_ptr<AlertIndicatorButton> alert_indicator_;
  const raw_ptr<TabCloseButton> close_button_;
  raw_ptr<glic::TabUnderlineView> glic_tab_underline_view_ = nullptr;

  base::CallbackListSubscription node_destroyed_subscription_;
  base::CallbackListSubscription tab_state_changed_subscription_;
  base::CallbackListSubscription collapsed_state_changed_subscription_;
  base::CallbackListSubscription paint_as_active_subscription_;
  base::CallbackListSubscription ax_name_changed_subscription_;
  base::CallbackListSubscription tab_data_changed_subscription_;

  tabs::TabData tab_data_;
  bool active_ = false;
  bool selected_ = false;
  bool hovered_ = false;
  bool split_ = false;
  bool collapsed_ = false;
  bool pinned_ = false;
  bool shift_pressed_on_mouse_down_ = false;
  bool should_fill_background_tab_color_ = false;

  std::unique_ptr<GlowHoverController> hover_controller_;
  float hover_opacity_min_;
  float hover_opacity_max_;
  float radial_highlight_opacity_;

  std::optional<performance_manager::freezing::FreezingVote>& GetFreezingVote(
      FreezingVoteReason reason);

  // Freezing vote held while the tab's group is collapsed.
  std::optional<performance_manager::freezing::FreezingVote>
      collapsed_freezing_vote_;
  // Freezing vote held while another group is focused in focus mode.
  std::optional<performance_manager::freezing::FreezingVote>
      focus_mode_freezing_vote_;

  std::unique_ptr<tabs::TabDataObserver> tab_data_observer_;

  base::ScopedObservation<views::View, views::ViewObserver>
      close_button_observation_{this};

  base::WeakPtrFactory<TabView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COMMON_TAB_VIEW_H_
