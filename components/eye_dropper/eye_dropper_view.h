// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EYE_DROPPER_EYE_DROPPER_VIEW_H_
#define COMPONENTS_EYE_DROPPER_EYE_DROPPER_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#endif

namespace eye_dropper {

// EyeDropperView is used on Aura platforms.
class EyeDropperView : public content::EyeDropper,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                       public aura::WindowObserver,
#endif
                       public views::WidgetDelegateView {
 public:
  METADATA_HEADER(EyeDropperView);
  EyeDropperView(gfx::NativeView parent,
                 gfx::NativeView event_handler,
                 content::EyeDropperListener* listener);
  EyeDropperView(const EyeDropperView&) = delete;
  EyeDropperView& operator=(const EyeDropperView&) = delete;
  ~EyeDropperView() override;

 protected:
  // views::WidgetDelegateView:
  void OnPaint(gfx::Canvas* canvas) override;
  void WindowClosing() override;
  void OnWidgetMove() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // aura::WindowObserver:
  void OnWindowAddedToRootWindow(aura::Window* window) override;
#endif

 private:
  class ViewPositionHandler;
  class ScreenCapturer;

  class PreEventDispatchHandler : public ui::EventHandler {
   public:
    PreEventDispatchHandler(EyeDropperView* view,
                            gfx::NativeView event_handler);
    PreEventDispatchHandler(const PreEventDispatchHandler&) = delete;
    PreEventDispatchHandler& operator=(const PreEventDispatchHandler&) = delete;
    ~PreEventDispatchHandler() override;

   private:
    void OnMouseEvent(ui::MouseEvent* event) override;
    void OnGestureEvent(ui::GestureEvent* event) override;

    raw_ptr<EyeDropperView> view_;
#if defined(USE_AURA)
    class KeyboardHandler;
    class FocusObserver;
    std::unique_ptr<KeyboardHandler> keyboard_handler_;
    std::unique_ptr<FocusObserver> focus_observer_;
#endif
  };

  void CaptureScreen(absl::optional<webrtc::DesktopCapturer::SourceId> screen);

  // Moves the view to the cursor position.
  void UpdatePosition();

  void MoveViewToFront();

  void CaptureInputIfNeeded();

  void HideCursor();
  void ShowCursor();

  // Handles color selection and notifies the listener.
  void OnColorSelected();
  void OnColorSelectionCanceled();

  gfx::Size GetSize() const;
  float GetDiameter() const;

  // Receives the color selection.
  raw_ptr<content::EyeDropperListener> listener_;

  std::unique_ptr<PreEventDispatchHandler> pre_dispatch_handler_;
  std::unique_ptr<ViewPositionHandler> view_position_handler_;
  std::unique_ptr<ScreenCapturer> screen_capturer_;
  absl::optional<SkColor> selected_color_;
  base::TimeTicks ignore_selection_time_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // When the widget moves across displays we update the screenshot.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
#endif
};

}  // namespace eye_dropper

#endif  // COMPONENTS_EYE_DROPPER_EYE_DROPPER_VIEW_H_
