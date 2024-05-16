// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROL_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROL_BUTTON_H_

#include "base/memory/raw_ref.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/masked_targeter_delegate.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

class TabStripController;

enum class Edge {
  kNone = 0,
  kLeft,
  kRight,
};

class TabStripControlButton : public views::LabelButton,
                              public views::MaskedTargeterDelegate {
  METADATA_HEADER(TabStripControlButton, views::LabelButton)

 public:
  static const int kIconSize;
  static const gfx::Size kButtonSize;

  TabStripControlButton(TabStripController* tab_strip,
                        PressedCallback callback,
                        const gfx::VectorIcon& icon,
                        Edge flat_edge = Edge::kNone);

  TabStripControlButton(TabStripController* tab_strip,
                        PressedCallback callback,
                        const std::u16string& text,
                        Edge flat_edge = Edge::kNone);

  TabStripControlButton(TabStripController* tab_strip,
                        PressedCallback callback,
                        const gfx::VectorIcon& icon,
                        const std::u16string& text,
                        Edge flat_edge = Edge::kNone);

  TabStripControlButton(const TabStripControlButton&) = delete;
  TabStripControlButton& operator=(const TabStripControlButton&) = delete;
  ~TabStripControlButton() override = default;

  // Update the Colors of the button.
  void SetForegroundFrameActiveColorId(ui::ColorId new_color_id);
  void SetForegroundFrameInactiveColorId(ui::ColorId new_color_id);
  void SetBackgroundFrameActiveColorId(ui::ColorId new_color_id);
  void SetBackgroundFrameInactiveColorId(ui::ColorId new_color_id);

  // Updates the icon model.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Updates the styling and icons for the button. Should be called when colors
  // change.
  void UpdateIcon();

  virtual int GetCornerRadius() const;
  virtual int GetFlatCornerRadius() const;
  float GetScaledCornerRadius(float initial_radius, Edge edge) const;

  Edge flat_edge() { return flat_edge_; }
  float flat_edge_factor_for_testing() { return flat_edge_factor_; }

  void SetFlatEdgeFactor(float factor);

  // Helper function for changing the state for TabStripRegionView tests.
  void AnimateToStateForTesting(views::InkDropState state);

  // views::View
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnThemeChanged() override;

  // views::MaskedTargeterDelegate
  bool GetHitTestMask(SkPath* mask) const override;

 protected:
  // Returns colors based on the Frame active status.
  ui::ColorId GetBackgroundColor();
  ui::ColorId GetForegroundColor();

  // Called whenever a color change occurs (theming/frame state). By default
  // this only changes the hover color and updates the icon. Override for any
  // additional changes.
  virtual void UpdateColors();

  // views::Button
  void NotifyClick(const ui::Event& event) override;

  void set_paint_transparent_for_custom_image_theme(
      bool paint_transparent_for_custom_image_theme) {
    paint_transparent_for_custom_image_theme_ =
        paint_transparent_for_custom_image_theme;
  }

 private:
  void UpdateBackground();
  void UpdateInkDrop();

  // Optional icon for the label button.
  raw_ref<const gfx::VectorIcon> icon_;

  bool paint_transparent_for_custom_image_theme_ = false;

  // Button edge which should render without rounded corners.
  Edge flat_edge_;

  // Corner radius multiplier on the corners adjacent to the flat edge, if any.
  // Between 0-1, where corners will be flat at 0 and rounded at 1. Used for
  // animating corner radius.
  float flat_edge_factor_ = 1;

  // Tab strip that contains this button.
  raw_ptr<TabStripController, AcrossTasksDanglingUntriaged>
      tab_strip_controller_;

  // Stored ColorId values to differentiate for ChromeRefresh.
  ui::ColorId foreground_frame_active_color_id_;
  ui::ColorId foreground_frame_inactive_color_id_;
  ui::ColorId background_frame_active_color_id_;
  ui::ColorId background_frame_inactive_color_id_;

  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROL_BUTTON_H_
