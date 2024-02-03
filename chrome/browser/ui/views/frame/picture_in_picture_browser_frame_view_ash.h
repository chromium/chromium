// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_ASH_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "ui/aura/window_observer.h"

class BrowserView;
class BrowserFrame;

// PictureInPictureBrowserFrameViewAsh provides the NonClientFrameView for pip
// windows on ChromeOS under classic ash.
class PictureInPictureBrowserFrameViewAsh
    : public PictureInPictureBrowserFrameView,
      public aura::WindowObserver {
 public:
  PictureInPictureBrowserFrameViewAsh(BrowserFrame* frame,
                                      BrowserView* browser_view);

  PictureInPictureBrowserFrameViewAsh(
      const PictureInPictureBrowserFrameViewAsh&) = delete;
  PictureInPictureBrowserFrameViewAsh& operator=(
      const PictureInPictureBrowserFrameViewAsh&) = delete;

  ~PictureInPictureBrowserFrameViewAsh() override;

  // BrowserNonClientFrameView:
  void UpdateWindowRoundedCorners() override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroyed(aura::Window* window) override;

 protected:
  // PictureInPictureBrowserFrameView:
  gfx::Insets ResizeBorderInsets() const override;

 private:
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_PICTURE_IN_PICTURE_BROWSER_FRAME_VIEW_ASH_H_
