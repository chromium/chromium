// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_CHROMEOS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"

class ImmersiveModeControllerChromeos
    : public ImmersiveModeController,
      public chromeos::ImmersiveFullscreenControllerDelegate,
      public FullscreenObserver,
      public aura::WindowObserver {
 public:
  ImmersiveModeControllerChromeos();

  ImmersiveModeControllerChromeos(const ImmersiveModeControllerChromeos&) =
      delete;
  ImmersiveModeControllerChromeos& operator=(
      const ImmersiveModeControllerChromeos&) = delete;

  ~ImmersiveModeControllerChromeos() override;

  chromeos::ImmersiveFullscreenController* controller() { return &controller_; }

  // ImmersiveModeController overrides:
  void Init(BrowserView* browser_view) override;
  void SetEnabled(bool enabled) override;
  bool IsEnabled() const override;
  bool ShouldHideTopViews() const override;
  bool IsRevealed() const override;
  int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const override;
  std::unique_ptr<ImmersiveRevealedLock> GetRevealedLock(
      AnimateReveal animate_reveal) override;
  void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) override;
  bool ShouldStayImmersiveAfterExitingFullscreen() override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  int GetMinimumContentOffset() const override;
  int GetExtraInfobarOffset() const override;
  void OnContentFullscreenChanged(bool is_content_fullscreen) override;

 private:
  // Updates the browser root view's layout including window caption controls.
  void LayoutBrowserRootView();

  // ImmersiveFullscreenController::Delegate overrides:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenEntered() override;
  void OnImmersiveFullscreenExited() override;
  void SetVisibleFraction(double visible_fraction) override;
  std::vector<gfx::Rect> GetVisibleBoundsInScreen() const override;

  // FullscreenObserver:
  void OnFullscreenStateChanged() override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  chromeos::ImmersiveFullscreenController controller_;

  raw_ptr<BrowserView> browser_view_ = nullptr;

  // The current visible bounds of the find bar, in screen coordinates. This is
  // an empty rect if the find bar is not visible.
  gfx::Rect find_bar_visible_bounds_in_screen_;

  // The fraction of the TopContainerView's height which is visible. Zero when
  // the top-of-window views are not revealed.
  double visible_fraction_ = 1.0;

  base::ScopedObservation<FullscreenController, FullscreenObserver>
      fullscreen_observer_{this};

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_CHROMEOS_H_
