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

class TabStrip;

class TabStripControlButton : public views::LabelButton,
                              public views::MaskedTargeterDelegate {
 public:
  METADATA_HEADER(TabStripControlButton);

  static const int kIconSize;
  static const gfx::Size kButtonSize;

  TabStripControlButton(TabStrip* tab_strip,
                        PressedCallback callback,
                        const gfx::VectorIcon& icon);
  TabStripControlButton(const TabStripControlButton&) = delete;
  TabStripControlButton& operator=(const TabStripControlButton&) = delete;
  ~TabStripControlButton() override = default;

  // Updates the styling and icons for the button. Should be called when colors
  // change.
  void UpdateIcon();

  virtual int GetCornerRadius() const;

  // Helper function for changing the state for TabStripRegionView tests.
  void AnimateToStateForTesting(views::InkDropState state);

  // views::View
  gfx::Size CalculatePreferredSize() const override;
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

  void UpdateForegroundFrameActiveColorId(ui::ColorId new_color_id) {
    foreground_frame_active_color_id_ = new_color_id;
    UpdateColors();
  }
  void UpdateForegroundFrameInactiveColorId(ui::ColorId new_color_id) {
    foreground_frame_inactive_color_id_ = new_color_id;
    UpdateColors();
  }
  void UpdateBackgroundFrameActiveColorId(ui::ColorId new_color_id) {
    background_frame_active_color_id_ = new_color_id;
    UpdateColors();
  }
  void UpdateBackgroundFrameInactiveColorId(ui::ColorId new_color_id) {
    background_frame_inactive_color_id_ = new_color_id;
    UpdateColors();
  }

  bool GetPaintTransparentForCustomImageTheme() {
    return paint_transparent_for_custom_image_theme_;
  }

  void SetPaintTransparentForCustomImageTheme(
      bool paint_transparent_for_custom_image_theme) {
    paint_transparent_for_custom_image_theme_ =
        paint_transparent_for_custom_image_theme;
  }

 private:
  void UpdateBackground();
  void UpdateInkDrop();

  // Icon for the label button.
  const raw_ref<const gfx::VectorIcon> icon_;

  bool paint_transparent_for_custom_image_theme_;

  // Tab strip that contains this button.
  raw_ptr<TabStrip, AcrossTasksDanglingUntriaged> tab_strip_;

  // Stored ColorId values to differentiate for ChromeRefresh.
  ui::ColorId foreground_frame_active_color_id_;
  ui::ColorId foreground_frame_inactive_color_id_;
  ui::ColorId background_frame_active_color_id_;
  ui::ColorId background_frame_inactive_color_id_;

  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_CONTROL_BUTTON_H_
