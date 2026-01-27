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
#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/tab_context_menu_controller.h"
#include "chrome/common/buildflags.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/node_hash_map.h"
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

#if BUILDFLAG(ENABLE_GLIC)
namespace glic {
class TabUnderlineView;
}
#endif

// View for a vertical tabstrip's tab.
class VerticalTabView : public views::View,
                        public views::LayoutDelegate,
                        public views::MaskedTargeterDelegate,
                        public AlertIndicatorButton::Delegate,
                        public views::ContextMenuController {
  METADATA_HEADER(VerticalTabView, views::View)

 public:
  explicit VerticalTabView(TabCollectionNode* collection_node);
  VerticalTabView(const VerticalTabView&) = delete;
  VerticalTabView& operator=(const VerticalTabView&) = delete;
  ~VerticalTabView() override;

  void StepLoadingAnimation(const base::TimeDelta& elapsed_time);
  void UpdateHovered(bool hovered);

  std::optional<SkColor> GetBackgroundColor();

  const TabCollectionNode* collection_node() const { return collection_node_; }
  const TabStyle* tab_style() { return tab_style_; }
  const TabRendererData& tab_data() const { return tab_data_; }
  float radial_highlight_opacity() { return radial_highlight_opacity_; }

  TabCloseButton* close_button_for_testing() { return close_button_; }
  bool collapsed_for_testing() { return collapsed_; }
  SkPath GetPath() const;

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
  void OnPaint(gfx::Canvas* canvas) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnThemeChanged() override;

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

  void OnDataChanged();

  void UpdateTitle();

  void UpdateBorder();

  // Calculates the visibilities of child views based on various states.
  absl::node_hash_map<views::View*, bool> CalculateChildVisibilities() const;

  void UpdateColors();
  void UpdateContrastRatioValues();

  void CloseButtonPressed(const ui::Event& event);

  bool IsHoverAnimationActive() const;
  double GetHoverAnimationValue() const;
  float GetHoverOpacity() const;

  bool IsFrameActive() const;
  TabStyle::TabSelectionState GetSelectionState() const;

  const tabs::TabInterface* GetTabInterface() const;

  raw_ptr<TabCollectionNode> collection_node_ = nullptr;

  std::vector<TabChildConfig> tab_children_configs_;

  const raw_ptr<const TabStyle> tab_style_;

  const raw_ptr<TabIcon> icon_;
  const raw_ptr<views::Label> title_;
  const raw_ptr<AlertIndicatorButton> alert_indicator_;
  const raw_ptr<TabCloseButton> close_button_;

#if BUILDFLAG(ENABLE_GLIC)
  raw_ptr<glic::TabUnderlineView> glic_tab_underline_view_ = nullptr;
#endif

  base::CallbackListSubscription node_destroyed_subscription_;
  base::CallbackListSubscription data_changed_subscription_;
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

  base::WeakPtrFactory<VerticalTabView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_
