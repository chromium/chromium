// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTROLS_SLIDE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTROLS_SLIDE_CONTROLLER_H_

namespace content {
class WebContents;
}  // namespace content

// Defines an interface for a controller that implements the Android-like
// browser top controls (a.k.a. top-chrome) sliding behavior when the current
// tab's page is scrolled by touch gestures.
// https://crbug.com/856222.
class TopControlsSlideController {
 public:
  TopControlsSlideController() = default;

  TopControlsSlideController(const TopControlsSlideController&) = delete;
  TopControlsSlideController& operator=(const TopControlsSlideController&) =
      delete;

  virtual ~TopControlsSlideController() = default;

  // Returns true when the browser top controls slide behavior with page scrolls
  // is enabled, i.e. when in tablet mode and browser window is non-immersive.
  virtual bool IsEnabled() const = 0;

  // Returns the current shown ratio of the browser controls.
  virtual float GetShownRatio() const = 0;

  // Sets the top controls UIs shown ratio as a result of page scrolling in
  // |contents|. The shown ratio is a value in the range [0.f, 1.f], where 0 is
  // fully hidden, and 1 is fully shown.
  virtual void SetShownRatio(content::WebContents* contents, float ratio) = 0;

  // Inform the controller that the browser is about to change its fullscreen
  // state, potentially enabling immersive fullscreen mode which should disable
  // the top controls slide behavior with page scrolls.
  virtual void OnBrowserFullscreenStateWillChange(
      bool new_fullscreen_state) = 0;

  // Whether or not the renderer's viewport size has been shrunk by the height
  // of the browser's top controls.
  // See BrowserWindow::DoBrowserControlsShrinkRendererSize() for more details.
  virtual bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const = 0;

  // Called from the renderer to inform the controller that gesture scrolling
  // changed state.
  virtual void SetTopControlsGestureScrollInProgress(bool in_progress) = 0;

  // Returns true while gesture scrolls are in progress.
  virtual bool IsTopControlsGestureScrollInProgress() const = 0;

  // Returns true while the top controls are sliding up or down, and hasn't
  // reached a final steady state.
  virtual bool IsTopControlsSlidingInProgress() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTROLS_SLIDE_CONTROLLER_H_
