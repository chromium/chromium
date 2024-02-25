// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_container_loading_bar.h"

#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"

LoadingBarView::LoadingBarView() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  animation_.SetDuration(base::Milliseconds(300));
}

double LoadingBarView::GetDisplayedLoadingProgress() const {
  return gfx::Tween::DoubleValueBetween(
      gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT,
                                 animation_.GetCurrentValue()),
      start_loading_progress_, target_loading_progress_);
}

void LoadingBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

void LoadingBarView::AddedToWidget() {
  SchedulePaint();
}

void LoadingBarView::HideImmediately() {
  is_shown_when_not_animating_ = false;
  start_loading_progress_ = 0.0;
  target_loading_progress_ = 0.0;
  animation_.Stop();
  // If we were previously drawn we have to redraw as invisible.
  SchedulePaint();
}

void LoadingBarView::Show(double loading_progress) {
  is_shown_when_not_animating_ = true;
  start_loading_progress_ = loading_progress;
  target_loading_progress_ = loading_progress;
  animation_.Stop();
  SchedulePaint();
}

void LoadingBarView::FinishLoading() {
  if (!is_shown_when_not_animating_)
    return;
  SetLoadingProgress(1.0);
  is_shown_when_not_animating_ = false;
}

void LoadingBarView::SetLoadingProgress(double loading_progress) {
  if (loading_progress <= target_loading_progress_)
    return;
  start_loading_progress_ = GetDisplayedLoadingProgress();
  target_loading_progress_ = loading_progress;
  animation_.SetCurrentValue(0.0);
  animation_.Start();
}

void LoadingBarView::OnPaint(gfx::Canvas* canvas) {
  if (is_shown_when_not_animating_ || animation_.is_animating()) {
    const auto* const color_provider = GetColorProvider();
    canvas->FillRect(
        GetLocalBounds(),
        color_provider->GetColor(kColorTabstripLoadingProgressBackground));
    gfx::Rect progress_bounds(GetLocalBounds());
    progress_bounds.set_width(gfx::Tween::IntValueBetween(
        GetDisplayedLoadingProgress(), 0, progress_bounds.width()));
    canvas->FillRect(
        progress_bounds,
        color_provider->GetColor(kColorTabstripLoadingProgressForeground));
  }
}

void LoadingBarView::AnimationEnded(const gfx::Animation* animation) {
  SchedulePaint();
}

void LoadingBarView::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

BEGIN_METADATA(LoadingBarView)
END_METADATA

TopContainerLoadingBar::TopContainerLoadingBar(Browser* browser)
    : browser_(browser) {}

void TopContainerLoadingBar::SetWebContents(
    content::WebContents* web_contents) {
  Observe(web_contents);

  if (!web_contents) {
    network_state_ = TabNetworkState::kNone;
    HideImmediately();
    return;
  }

  // TODO(pbos): Consider storing one loading bar per tab and have it run (and
  // observing) in the background. This would remove the need to reset
  // loading-bar state during tab transitions as we'd just swap in the visible
  // object. Currently Show(GetLoadingProgress()) can decrease from what was
  // previously displayed in that tab.

  // Reset network state to update from a clean slate.
  network_state_ = TabNetworkState::kNone;
  UpdateLoadingProgress();
}

void TopContainerLoadingBar::UpdateLoadingProgress() {
  DCHECK(web_contents());
  if (!browser_->ShouldDisplayFavicon(web_contents())) {
    HideImmediately();
    return;
  }

  TabUIHelper* const tab_ui_helper =
      TabUIHelper::FromWebContents(web_contents());
  if (tab_ui_helper->ShouldHideThrobber()) {
    HideImmediately();
    return;
  }

  const TabNetworkState old_network_state = network_state_;
  network_state_ = TabNetworkStateForWebContents(web_contents());
  if (old_network_state != network_state_) {
    if (network_state_ == TabNetworkState::kWaiting ||
        network_state_ == TabNetworkState::kLoading) {
      // Reset loading state when we go to waiting or loading.
      Show(GetLoadingProgress());
    }
  }

  switch (network_state_) {
    case TabNetworkState::kLoading:
      SetLoadingProgress(GetLoadingProgress());
      break;
    case TabNetworkState::kError:
      // TODO(pbos): Add a better error indicator (fade-out red?).
      HideImmediately();
      break;
    case TabNetworkState::kWaiting:
      break;
    case TabNetworkState::kNone:
      FinishLoading();
      break;
  }
}

double TopContainerLoadingBar::GetLoadingProgress() const {
  DCHECK(web_contents());
  return std::min(web_contents()->GetLoadProgress(), 0.9);
}

void TopContainerLoadingBar::LoadProgressChanged(double progress) {
  UpdateLoadingProgress();
}

BEGIN_METADATA(TopContainerLoadingBar)
ADD_READONLY_PROPERTY_METADATA(double, LoadingProgress)
END_METADATA
