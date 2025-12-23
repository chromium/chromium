// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_

#include "base/callback_list.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"
#include "chrome/browser/ui/views/tabs/tab_context_menu_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/view.h"

class GlowHoverController;
class TabCloseButton;
class TabCollectionNode;
class TabIcon;

namespace views {
class Label;
}

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

  void UpdateHovered(bool hovered);

  void OnTabDragOver();

  TabIcon* icon_for_testing() { return icon_; }
  AlertIndicatorButton* alert_indicator_for_testing() {
    return alert_indicator_;
  }
  TabCloseButton* close_button_for_testing() { return close_button_; }

  void OnMouseEnteredForTesting(const ui::MouseEvent& event) {
    OnMouseEntered(event);
  }
  void OnMouseExitedForTesting(const ui::MouseEvent& event) {
    OnMouseExited(event);
  }

 private:
  // views::View
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnThemeChanged() override;

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

  void UpdateBorder();

  void UpdateAlertIndicatorVisibility();
  void UpdateCloseButtonVisibility();

  void UpdateColors();
  void UpdateContrastRatioValues();

  void CloseButtonPressed(const ui::Event& event);

  bool IsHoverAnimationActive() const;
  double GetHoverAnimationValue() const;
  float GetHoverOpacity() const;

  SkPath GetPath() const;

  bool IsFrameActive() const;
  TabStyle::TabSelectionState GetSelectionState() const;

  const tabs::TabInterface* GetTabInterface() const;

  raw_ptr<TabCollectionNode> collection_node_ = nullptr;

  const raw_ptr<const TabStyle> tab_style_;

  const raw_ptr<TabIcon> icon_;
  const raw_ptr<views::Label> title_;
  const raw_ptr<AlertIndicatorButton> alert_indicator_;
  const raw_ptr<TabCloseButton> close_button_;

  base::CallbackListSubscription node_destroyed_subscription_;
  base::CallbackListSubscription data_changed_subscription_;
  base::CallbackListSubscription paint_as_active_subscription_;

  bool active_ = false;
  bool selected_ = false;
  bool hovered_ = false;
  bool split_ = false;
  bool pinned_ = false;
  bool shift_pressed_on_mouse_down_ = false;

  std::unique_ptr<GlowHoverController> hover_controller_;
  float hover_opacity_min_;
  float hover_opacity_max_;
  float radial_highlight_opacity_;

  base::WeakPtrFactory<VerticalTabView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_
