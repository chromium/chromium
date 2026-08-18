// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_FLAT_EDGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_FLAT_EDGE_BUTTON_H_

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/masked_targeter_delegate.h"

// Button class that can display a flat edge, likely when paired with a sibling
// button.
class TabStripFlatEdgeButton : public views::LabelButton,
                               public views::MaskedTargeterDelegate {
  METADATA_HEADER(TabStripFlatEdgeButton, views::LabelButton)
 public:
  enum class FlatEdge {
    kNone,
    kTop,
    kLeft,
    kBottom,
    kRight,
  };
  TabStripFlatEdgeButton();
  ~TabStripFlatEdgeButton() override;

  // views::LabelButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // views::MaskedTargeterDelegate
  bool GetHitTestMask(SkPath* mask) const override;

  void SetFlatEdge(FlatEdge flat_edge);
  void SetIconSize(int icon_size);
  void UpdateIcon(const ui::ImageModel& icon_image);
  void SetInsets(const gfx::Insets& insets);
  void SetIconOpacity(float opacity);

  void SetExpansionFactor(float factor);
  float GetExpansionFactor() const { return expansion_factor_; }

  void SetExpansionOrientation(views::LayoutOrientation orientation);

  void SetFlatEdgeFactor(float factor);
  float GetFlatEdgeFactor() const { return flat_edge_factor_; }

  void SetShouldShowLabel(bool show_label);

  // Sets the text that will be displayed when the label is shown.
  void SetLabelText(const std::u16string& text);

  base::CallbackListSubscription RegisterWillInvokeActionCallback(
      base::RepeatingClosure callback);
  void NotifyWillInvokeAction();

  FlatEdge flat_edge_for_testing() const { return flat_edge_; }

 private:
  // views::View:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  ui::ColorId GetForegroundColor() const;
  ui::ColorId GetBackgroundColor() const;
  SkRRect GetButtonShape() const;
  gfx::RoundedCornersF GetButtonCornerRadii() const;

  // Updates the label visibility and padding based on whether it should be
  // shown.
  void UpdateLabel(bool should_show);
  void UpdateLabelColor();

  // Whether the label should be shown, if there is space.
  bool should_show_label_ = false;
  std::u16string label_text_;
  int icon_size_ = 0;
  float expansion_factor_ = 1.0f;
  views::LayoutOrientation expansion_orientation_ =
      views::LayoutOrientation::kHorizontal;
  float flat_edge_factor_ = 1.0f;
  FlatEdge flat_edge_ = FlatEdge::kNone;
  base::CallbackListSubscription paint_as_active_subscription_;

  base::RepeatingClosureList will_invoke_action_callback_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_SHARED_TAB_STRIP_FLAT_EDGE_BUTTON_H_
