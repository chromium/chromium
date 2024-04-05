// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_HEADER_VIEW_H_
#define CHROMEOS_UI_FRAME_HEADER_VIEW_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}

namespace views {
class FrameCaptionButton;
class ImageView;
class Widget;
class NonClientFrameView;
}  // namespace views

namespace chromeos {

class DefaultFrameHeader;
class FrameCaptionButtonContainerView;

enum class FrameBackButtonState;

// View which paints the frame header (title, caption buttons...). It slides off
// and on screen in immersive fullscreen.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) HeaderView
    : public views::View,
      public chromeos::ImmersiveFullscreenControllerDelegate,
      public aura::WindowObserver,
      public display::DisplayObserver {
  METADATA_HEADER(HeaderView, views::View)

 public:
  // |target_widget| is the widget that the caption buttons act on.
  // |target_widget| is not necessarily the same as the widget the header is
  // placed in. For example, in immersive fullscreen this view may be painted in
  // a widget that slides in and out on top of the main app or browser window.
  // However, clicking a caption button should act on the target widget.
  HeaderView(views::Widget* target_widget,
             views::NonClientFrameView* frame_view);

  HeaderView(const HeaderView&) = delete;
  HeaderView& operator=(const HeaderView&) = delete;

  ~HeaderView() override;

  // Initialize the parts with side effects.
  void Init();

  void set_immersive_mode_changed_callback(base::RepeatingClosure callback) {
    immersive_mode_changed_callback_ = std::move(callback);
  }

  bool should_paint() { return should_paint_; }

  // Schedules a repaint for the entire title.
  void SchedulePaintForTitle();

  // Tells the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // Returns the amount of the view's pixels which should be on screen.
  int GetPreferredOnScreenHeight();

  // Returns the view's preferred height.
  int GetPreferredHeight();

  // Returns the view's minimum width.
  int GetMinimumWidth() const;

  // Sets the avatar icon to be displayed on the frame header.
  void SetAvatarIcon(const gfx::ImageSkia& avatar);

  void UpdateCaptionButtons();

  void SetWidthInPixels(int width_in_pixels);

  void SetHeaderCornerRadius(int radius);

  // views::View:
  void Layout(PassKey) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  bool IsDrawn() const override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  chromeos::FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

  views::View* avatar_icon() const;

  bool in_immersive_mode() const { return in_immersive_mode_; }
  bool is_revealed() const { return fullscreen_visible_fraction_ > 0.0; }

  void SetShouldPaintHeader(bool paint);

  views::FrameCaptionButton* GetBackButton();

  // ImmersiveFullscreenControllerDelegate:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenEntered() override;
  void OnImmersiveFullscreenExited() override;
  void SetVisibleFraction(double visible_fraction) override;
  std::vector<gfx::Rect> GetVisibleBoundsInScreen() const override;
  void Relayout() override;

  chromeos::DefaultFrameHeader* GetFrameHeader() { return frame_header_.get(); }

 private:
  class HeaderContentView;
  friend class HeaderContentView;

  // Paint the header content.
  void PaintHeaderContent(gfx::Canvas* canvas);

  void UpdateBackButton();
  void UpdateCenterButton();
  void UpdateCaptionButtonsVisibility();

  // The widget that the caption buttons act on.
  raw_ptr<views::Widget> target_widget_;

  // A callback to run when |in_immersive_mode_| changes.
  base::RepeatingClosure immersive_mode_changed_callback_;

  // Helper for painting the header.
  std::unique_ptr<chromeos::DefaultFrameHeader> frame_header_;

  raw_ptr<views::ImageView, DanglingUntriaged> avatar_icon_ = nullptr;

  // View which draws the content of the frame.
  raw_ptr<HeaderContentView> header_content_view_ = nullptr;

  // View which contains the window caption buttons.
  raw_ptr<chromeos::FrameCaptionButtonContainerView> caption_button_container_ =
      nullptr;

  // The fraction of the header's height which is visible while in fullscreen.
  // This value is meaningless when not in fullscreen.
  double fullscreen_visible_fraction_ = 0;

  // True if a layer should be used for the immersive mode reveal. Some code
  // needs HeaderView to always paint to a layer instead of only during
  // immersive reveal (see WmNativeWidgetAura).
  bool add_layer_for_immersive_ = false;

  bool did_layout_ = false;

  // False to skip painting. Used for overview mode to hide the header.
  bool should_paint_ = true;

  bool in_immersive_mode_ = false;

  // This is used to compute visible bounds.
  mutable bool is_drawn_override_ = false;

  // Observes property changes to |target_widget_|'s window.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};

  std::optional<display::ScopedDisplayObserver> display_observer_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_HEADER_VIEW_H_
