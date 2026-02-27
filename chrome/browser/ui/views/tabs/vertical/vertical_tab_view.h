// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/tabs/hover_card_anchor_target.h"
#include "chrome/browser/ui/views/tabs/tab/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"
#include "chrome/common/buildflags.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/view.h"

class GlowHoverController;
class TabCloseButton;
class TabCollectionNode;
class TabIcon;

namespace views {
class Label;
}

namespace tabs {
class VerticalTabStripStateController;
}

namespace glic {
class TabUnderlineView;
}

// The view class for the tab. It is responsible for painting the
// tab background and displaying the favicon, title, alert indicators and close
// button. It handles data changed event and view hierarchy changes to updates
// its states. The tab view implements its own layout and avoids using
// FlexLayout for performance reasons.
class VerticalTabView : public views::View,
                        public views::LayoutDelegate,
                        public views::MaskedTargeterDelegate,
                        public AlertIndicatorButton::Delegate,
                        public views::ContextMenuController,
                        public HoverCardAnchorTarget {
  METADATA_HEADER(VerticalTabView, views::View)

 public:
  explicit VerticalTabView(TabCollectionNode* collection_node);
  VerticalTabView(const VerticalTabView&) = delete;
  VerticalTabView& operator=(const VerticalTabView&) = delete;
  ~VerticalTabView() override;

  void StepLoadingAnimation(const base::TimeDelta& elapsed_time);

  void CreateFreezingVote();
  void ReleaseFreezingVote();
  bool HasFreezingVote() const { return freezing_vote_.has_value(); }

  void UpdateHovered(bool hovered);
  bool IsHoverAnimationActive() const;

  std::optional<SkColor> GetBackgroundColor();
  SkPath GetPath() const;

  const TabCollectionNode* collection_node() const { return collection_node_; }
  const TabStyle* tab_style() const { return tab_style_; }
  float radial_highlight_opacity() { return radial_highlight_opacity_; }

  TabCloseButton* close_button_for_testing() { return close_button_; }
  bool collapsed_for_testing() { return collapsed_; }

  // HoverCardAnchorTarget:
  bool IsActive() const override;
  bool IsValid() const override;
  const TabRendererData& data() const override;
  views::BubbleBorder::Arrow GetAnchorPosition() const override;

 private:
  // views::View
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
  void OnPaint(gfx::Canvas* canvas) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnFocus() override;
  void OnBlur() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnThemeChanged() override;

  // Tab Painting Helpers
  void PaintTabBackgroundWithImages(
      gfx::Canvas* canvas,
      std::optional<int> active_tab_fill_id,
      std::optional<int> inactive_tab_fill_id) const;
  float GetCurrentActiveOpacity() const;
  void PaintTabBackgroundFill(gfx::Canvas* canvas,
                              TabStyle::TabSelectionState selection_state,
                              bool hovered,
                              std::optional<int> fill_id) const;
  bool ShouldPaintTabBackgroundColor(
      TabStyle::TabSelectionState selection_state,
      bool has_custom_background,
      bool hovered) const;

  struct TabChildConfig {
    raw_ptr<views::View> view;
    int min_width;
    int padding;
    bool align_leading;
    bool expand;
  };

  gfx::Rect GetChildBounds(const gfx::Rect& container,
                           const TabChildConfig& config,
                           const bool center) const;

  // Calculates the visibilities of child views based on various states.
  absl::flat_hash_map<views::View*, bool> CalculateChildVisibilities() const;

  // views::LayoutDelegate
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

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
  void OnAXNameChanged(ax::mojom::StringAttribute attribute,
                       const std::optional<std::string>& name);
  void OnCollapsedStateChanged(
      tabs::VerticalTabStripStateController* controller);
  void OnDataChanged();
  void SetSelection(bool selected);
  void UpdateTabData(tabs::TabInterface* tab);

  void UpdateTitle();
  void UpdateBorder();
  void UpdateThemeColors();
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

  const tabs::TabInterface* GetTabInterface() const;

  raw_ptr<TabCollectionNode> collection_node_ = nullptr;

  std::vector<TabChildConfig> tab_children_configs_;

  const raw_ptr<const TabStyle> tab_style_;

  const raw_ptr<TabIcon> icon_;
  const raw_ptr<views::Label> title_;
  const raw_ptr<AlertIndicatorButton> alert_indicator_;
  const raw_ptr<TabCloseButton> close_button_;

  raw_ptr<glic::TabUnderlineView> glic_tab_underline_view_ = nullptr;

  base::CallbackListSubscription node_destroyed_subscription_;
  base::CallbackListSubscription data_changed_subscription_;
  base::CallbackListSubscription collapsed_state_changed_subscription_;
  base::CallbackListSubscription paint_as_active_subscription_;

  TabRendererData tab_data_;
  bool active_ = false;
  bool selected_ = false;
  bool hovered_ = false;
  bool split_ = false;
  bool collapsed_ = false;
  bool pinned_ = false;
  bool shift_pressed_on_mouse_down_ = false;

  std::unique_ptr<GlowHoverController> hover_controller_;
  float hover_opacity_min_;
  float hover_opacity_max_;
  float radial_highlight_opacity_;

  std::optional<int> active_tab_fill_id_;
  std::optional<int> inactive_tab_fill_id_;

  base::CallbackListSubscription ax_name_changed_subscription_;

  std::optional<performance_manager::freezing::FreezingVote> freezing_vote_;

  base::WeakPtrFactory<VerticalTabView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_
