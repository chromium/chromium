// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_LOADING_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_LOADING_BAR_H_

#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/view.h"

class Browser;

class LoadingBarView : public views::View, public gfx::AnimationDelegate {
 public:
  LoadingBarView();
  LoadingBarView(const LoadingBarView&) = delete;
  LoadingBarView& operator=(const LoadingBarView&) = delete;

  void HideImmediately();
  void Show(double loading_progress);
  void FinishLoading();

  void SetLoadingProgress(double loading_progress);

 private:
  double GetDisplayedLoadingProgress() const;

  // views::View:
  void OnThemeChanged() override;
  void AddedToWidget() override;
  void OnPaint(gfx::Canvas* canvas) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  gfx::LinearAnimation animation_{this};
  bool is_shown_when_not_animating_ = false;
  double start_loading_progress_ = 0.0;
  double target_loading_progress_ = 0.0;
};

class TopContainerLoadingBar : public LoadingBarView,
                               public content::WebContentsObserver {
 public:
  explicit TopContainerLoadingBar(Browser*);
  TopContainerLoadingBar(const TopContainerLoadingBar&) = delete;
  TopContainerLoadingBar& operator=(const TopContainerLoadingBar&) = delete;

  void SetWebContents(content::WebContents* web_contents);

 private:
  void UpdateLoadingProgress();
  double GetLoadingProgress();

  // content::WebContentsObserver:
  void LoadProgressChanged(double progress) override;

  Browser* browser_;
  TabNetworkState network_state_ = TabNetworkState::kNone;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTAINER_LOADING_BAR_H_
