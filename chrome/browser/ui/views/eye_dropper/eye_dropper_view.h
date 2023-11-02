// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/widget_delegate.h"

// EyeDropperView is used on Aura platforms and on the Mac before 10.15.
// Starting with macOS 10.15, EyeDropperViewMac is used as it relies on the new
// NSColorSampler API.
class EyeDropperView : public content::EyeDropper,
                       public views::WidgetDelegateView {
 public:
  METADATA_HEADER(EyeDropperView);
  EyeDropperView(content::RenderFrameHost* frame,
                 content::EyeDropperListener* listener);
  EyeDropperView(const EyeDropperView&) = delete;
  EyeDropperView& operator=(const EyeDropperView&) = delete;
  ~EyeDropperView() override;

 protected:
  // views::WidgetDelegateView:
  void OnPaint(gfx::Canvas* canvas) override;
  void WindowClosing() override;
  void OnWidgetMove() override;

 private:
  class ViewPositionHandler;
  class ScreenCapturer;

  class PreEventDispatchHandler : public ui::EventHandler {
   public:
    PreEventDispatchHandler(EyeDropperView* view, gfx::NativeView parent);
    PreEventDispatchHandler(const PreEventDispatchHandler&) = delete;
    PreEventDispatchHandler& operator=(const PreEventDispatchHandler&) = delete;
    ~PreEventDispatchHandler() override;

   private:
    void OnMouseEvent(ui::MouseEvent* event) override;

    raw_ptr<EyeDropperView> view_;
#if defined(USE_AURA)
    class KeyboardHandler;
    std::unique_ptr<KeyboardHandler> keyboard_handler_;
#endif
#if BUILDFLAG(IS_MAC)
    id clickEventTap_;
    id notificationObserver_;
#endif
  };

  // Moves the view to the cursor position.
  void UpdatePosition();

  void MoveViewToFront();

  void CaptureInputIfNeeded();

  void HideCursor();
  void ShowCursor();

  // Handles color selection and notifies the listener.
  void OnColorSelected();
  void OnColorSelectionCanceled();

  raw_ptr<content::RenderFrameHost> render_frame_host_;

  gfx::Size GetSize() const;
  float GetDiameter() const;

  // Receives the color selection.
  raw_ptr<content::EyeDropperListener> listener_;

  std::unique_ptr<PreEventDispatchHandler> pre_dispatch_handler_;
  std::unique_ptr<ViewPositionHandler> view_position_handler_;
  std::unique_ptr<ScreenCapturer> screen_capturer_;
  absl::optional<SkColor> selected_color_;
  base::TimeTicks ignore_selection_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EYE_DROPPER_EYE_DROPPER_VIEW_H_
