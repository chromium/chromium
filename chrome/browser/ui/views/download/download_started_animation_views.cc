// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/download/download_started_animation_views.h"

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

DownloadStartedAnimationViews::DownloadStartedAnimationViews(
    content::WebContents* web_contents,
    base::TimeDelta duration,
    const ui::ImageModel& image)
    : gfx::LinearAnimation(duration,
                           gfx::LinearAnimation::kDefaultFrameRate,
                           /*delegate=*/nullptr) {
  // If we're too small to show the download image, then don't bother.
  web_contents_bounds_ = web_contents->GetContainerBounds();
  if (WebContentsTooSmall(image.Size())) {
    return;
  }

  SetImage(image);

  popup_ = new views::Widget;

  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = false;
  params.parent = web_contents->GetNativeView();
  popup_->Init(std::move(params));
  popup_->SetOpacity(0.f);
  popup_->SetContentsView(this);
  popup_->Show();

  Start();
}

int DownloadStartedAnimationViews::GetWidth() const {
  return GetPreferredSize().width();
}

int DownloadStartedAnimationViews::GetHeight() const {
  return GetPreferredSize().height();
}

bool DownloadStartedAnimationViews::WebContentsTooSmall(
    const gfx::Size& image_size) const {
  return web_contents_bounds_.height() < image_size.height();
}

void DownloadStartedAnimationViews::Reposition() {
  popup_->SetBounds(gfx::Rect(GetX(), GetY(), GetWidth(), GetHeight()));
}

void DownloadStartedAnimationViews::Close() {
  popup_->Close();
}

void DownloadStartedAnimationViews::AnimateToState(double state) {
  if (state >= 1.0) {
    Close();
  } else {
    Reposition();
    popup_->SetOpacity(GetOpacity());
  }
}

BEGIN_METADATA(DownloadStartedAnimationViews)
END_METADATA
