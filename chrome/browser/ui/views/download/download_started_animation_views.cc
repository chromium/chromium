// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/download_started_animation.h"

#include "base/i18n/rtl.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

// How long to spend moving downwards and fading out after waiting.
constexpr auto kMoveTime = base::Milliseconds(600);

// The animation framerate.
const int kFrameRateHz = 60;

namespace {

// DownloadStartAnimation creates an animation (which begins running
// immediately) that animates an image downward from the center of the frame
// provided on the constructor, while simultaneously fading it out.  To use,
// simply call "new DownloadStartAnimation"; the class cleans itself up when it
// finishes animating.
class DownloadStartedAnimationViews : public gfx::LinearAnimation,
                                      public views::ImageView {
 public:
  METADATA_HEADER(DownloadStartedAnimationViews);
  explicit DownloadStartedAnimationViews(content::WebContents* web_contents);
  DownloadStartedAnimationViews(const DownloadStartedAnimationViews&) = delete;
  DownloadStartedAnimationViews& operator=(
      const DownloadStartedAnimationViews&) = delete;

 private:
  // Move the animation to wherever it should currently be.
  void Reposition();

  // Shut down the animation cleanly.
  void Close();

  // Animation
  void AnimateToState(double state) override;

  // We use a TYPE_POPUP for the popup so that it may float above any windows in
  // our UI.
  raw_ptr<views::Widget> popup_;

  // The content area at the start of the animation. We store this so that the
  // download shelf's resizing of the content area doesn't cause the animation
  // to move around. This means that once started, the animation won't move
  // with the parent window, but it's so fast that this shouldn't cause too
  // much heartbreak.
  gfx::Rect web_contents_bounds_;
};

DownloadStartedAnimationViews::DownloadStartedAnimationViews(
    content::WebContents* web_contents)
    : gfx::LinearAnimation(kMoveTime, kFrameRateHz, nullptr), popup_(nullptr) {
  auto download_image = ui::ImageModel::FromVectorIcon(
      kFileDownloadShelfIcon, kColorDownloadStartedAnimationForeground, 72);

  // If we're too small to show the download image, then don't bother -
  // the shelf will be enough.
  web_contents_bounds_ = web_contents->GetContainerBounds();
  if (web_contents_bounds_.height() < download_image.Size().height())
    return;

  SetImage(download_image);

  popup_ = new views::Widget;

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = false;
  params.parent = web_contents->GetNativeView();
  popup_->Init(std::move(params));
  popup_->SetOpacity(0.f);
  popup_->SetContentsView(this);
  Reposition();
  popup_->Show();

  Start();
}

void DownloadStartedAnimationViews::Reposition() {
  // Align the image with the bottom left of the web contents (so that it
  // points to the newly created download).
  gfx::Size size = GetPreferredSize();
  int x = base::i18n::IsRTL() ?
      web_contents_bounds_.right() - size.width() : web_contents_bounds_.x();
  popup_->SetBounds(gfx::Rect(
      x,
      static_cast<int>(web_contents_bounds_.bottom() -
          size.height() - size.height() * (1 - GetCurrentValue())),
      size.width(),
      size.height()));
}

void DownloadStartedAnimationViews::Close() {
  popup_->Close();
}

void DownloadStartedAnimationViews::AnimateToState(double state) {
  if (state >= 1.0) {
    Close();
  } else {
    Reposition();

    // Start at zero, peak halfway and end at zero.
    popup_->SetOpacity(static_cast<float>(
        std::min(1.0 - pow(GetCurrentValue() - 0.5, 2) * 4.0, 1.0)));
  }
}

BEGIN_METADATA(DownloadStartedAnimationViews, views::ImageView)
END_METADATA

}  // namespace

// static
void DownloadStartedAnimation::Show(content::WebContents* web_contents) {
  // The animation will delete itself when it's finished.
  new DownloadStartedAnimationViews(web_contents);
}
