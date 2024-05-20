// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/views/chrome_views_delegate.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/frame/non_client_frame_view_base.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace {

// A NonClientFrameView for framed lacros widgets supporting immersive
// fullscreen.
class NonClientFrameViewLacros : public chromeos::NonClientFrameViewBase,
                                 public aura::WindowObserver {
  METADATA_HEADER(NonClientFrameViewLacros, chromeos::NonClientFrameViewBase)

 public:
  explicit NonClientFrameViewLacros(views::Widget* frame)
      : NonClientFrameViewBase(frame) {
    window_observation_.Observe(frame->GetNativeWindow());
    immersive_fullscreen_controller_.Init(GetHeaderView(), frame,
                                          GetHeaderView());
  }
  NonClientFrameViewLacros(const NonClientFrameViewLacros&) = delete;
  NonClientFrameViewLacros& operator=(const NonClientFrameViewLacros&) = delete;
  ~NonClientFrameViewLacros() override = default;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key == chromeos::kWindowStateTypeKey) {
      const bool is_fullscreen =
          window->GetProperty(chromeos::kWindowStateTypeKey) ==
          chromeos::WindowStateType::kFullscreen;
      chromeos::ImmersiveFullscreenController::EnableForWidget(frame_,
                                                               is_fullscreen);
    }
  }
  void OnWindowDestroying(aura::Window* window) override {
    window_observation_.Reset();
  }

 private:
  chromeos::ImmersiveFullscreenController immersive_fullscreen_controller_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

BEGIN_METADATA(NonClientFrameViewLacros)
END_METADATA

}  // namespace

std::unique_ptr<views::NonClientFrameView>
ChromeViewsDelegate::CreateDefaultNonClientFrameView(views::Widget* widget) {
  return std::make_unique<NonClientFrameViewLacros>(widget);
}

bool ChromeViewsDelegate::ShouldWindowHaveRoundedCorners(
    gfx::NativeWindow window) const {
  return chromeos::ShouldWindowHaveRoundedCorners(window);
}
