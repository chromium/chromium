// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROL_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROL_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/masked_targeter_delegate.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

class BrowserFrameView;
class BrowserWindowInterface;

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

  TabStripControlButton(BrowserWindowInterface* browser_window_interface,
                        PressedCallback callback,
                        const gfx::VectorIcon& icon,
                        Edge fixed_flat_edge = Edge::kNone,
                        Edge animated_flat_edge = Edge::kNone);

  TabStripControlButton(BrowserWindowInterface* browser_window_interface,
                        PressedCallback callback,
                        const std::u16string& text,
                        Edge fixed_flat_edge = Edge::kNone,
                        Edge animated_flat_edge = Edge::kNone);

  TabStripControlButton(BrowserWindowInterface* browser_window_interface,
                        PressedCallback callback,
                        const gfx::VectorIcon& icon,
                        const std::u16string& text,
                        Edge fixed_flat_edge = Edge::kNone,
                        Edge animated_flat_edge = Edge::kNone);

  TabStripControlButton(const TabStripControlButton&) = delete;
  TabStripControlButton& operator=(const TabStripControlButton&) = delete;
  ~TabStripControlButton() override = default;

  // Update the Colors of the button.
  void SetForegroundFrameActiveColorId(ui::ColorId new_color_id);
  void SetForegroundFrameInactiveColorId(ui::ColorId new_color_id);
  void SetBackgroundFrameActiveColorId(ui::ColorId new_color_id);
  void SetBackgroundFrameInactiveColorId(ui::ColorId new_color_id);

  // Set custom ColorIds for the InkDrop hover and ripple effects.
  void SetInkdropHoverColorId(const ChromeColorIds new_color_id);
  void SetInkdropRippleColorId(const ChromeColorIds new_color_id);

  // Updates the icon model.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Updates the styling and icons for the button. Should be called when colors
  // change.
  void UpdateIcon();

  virtual int GetCornerRadius() const;
  virtual int GetFlatCornerRadius() const;
  float GetScaledCornerRadius(float initial_radius, Edge edge) const;

  // Optionally set the left and right corner radius individually and update the
  // background. Both default to the value set in GetCornerRadius if not set.
  void SetLeftRightCornerRadii(int left, int right);

  int GetLeftCornerRadius() const {
    return left_corner_radius_.value_or(GetCornerRadius());
  }

  int GetRightCornerRadius() const {
    return right_corner_radius_.value_or(GetCornerRadius());
  }

  Edge animated_flat_edge() { return animated_flat_edge_; }
  float flat_edge_factor_for_testing() { return flat_edge_factor_; }

  void SetFlatEdgeFactor(float factor);

  // Helper function for changing the state for HorizontalTabStripRegionView
  // tests.
  void AnimateToStateForTesting(views::InkDropState state);

  // views::View
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnThemeChanged() override;

  // views::MaskedTargeterDelegate
  bool GetHitTestMask(SkPath* mask) const override;

  // views::LabelButton
  void SetText(std::u16string_view text) override;

  // Returns colors based on the Frame active status.
  ui::ColorId GetBackgroundColor();
  ui::ColorId GetForegroundColor();

 protected:
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

  // See BrowserFrameView::IsFrameCondensed(). This accessor is made virtual for
  // testing.
  virtual bool IsFrameCondensed() const;

 private:
  void UpdateBackground();
  void UpdateInkDrop();

  bool IsWidgetAlive() const;

  BrowserFrameView* GetBrowserFrameView() const;

  // Optional icon for the label button.
  raw_ref<const gfx::VectorIcon> icon_;

  bool paint_transparent_for_custom_image_theme_ = false;

  // Button edge which should always render without rounded corners.
  Edge fixed_flat_edge_;

  // Button edge which should sometimes render without rounded corners,
  // depending on animation state.
  Edge animated_flat_edge_;

  // Corner radius multiplier on the corners adjacent to the animated flat
  // edge, if any. Between 0-1, where corners will be flat at 0 and rounded at
  // 1. Used for animating corner radius.
  float flat_edge_factor_ = 1;

  // Browser that contains this button.
  raw_ptr<BrowserWindowInterface> browser_window_interface_;

  // Stored ColorId values to differentiate for ChromeRefresh.
  ui::ColorId foreground_frame_active_color_id_;
  ui::ColorId foreground_frame_inactive_color_id_;
  ui::ColorId background_frame_active_color_id_;
  ui::ColorId background_frame_inactive_color_id_;

  // ColorIds used for the InkDrop hover and ripple effects.
  ChromeColorIds inkdrop_hover_color_id_;
  ChromeColorIds inkdrop_ripple_color_id_;

  // Optional radii for setting different edges on each side of the button.
  std::optional<int> left_corner_radius_;
  std::optional<int> right_corner_radius_;

  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROL_BUTTON_H_
