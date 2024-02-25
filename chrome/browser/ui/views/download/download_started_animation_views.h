// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_STARTED_ANIMATION_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_STARTED_ANIMATION_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

// DownloadStartedAnimationViews creates an animation (which begins running
// immediately) that animates an image within the frame provided on the
// constructor. To use, simply instantiate a subclass using "new"; the class
// cleans itself up when it finishes animating.
class DownloadStartedAnimationViews : public gfx::LinearAnimation,
                                      public views::ImageView {
  METADATA_HEADER(DownloadStartedAnimationViews, views::ImageView)

 public:
  DownloadStartedAnimationViews(content::WebContents* web_contents,
                                base::TimeDelta duration,
                                const ui::ImageModel& image);
  DownloadStartedAnimationViews(const DownloadStartedAnimationViews&) = delete;
  DownloadStartedAnimationViews& operator=(
      const DownloadStartedAnimationViews&) = delete;
  ~DownloadStartedAnimationViews() override = default;

 protected:
  const gfx::Rect& web_contents_bounds() const { return web_contents_bounds_; }

 private:
  // Compute the position of the image for the current state.
  virtual int GetX() const = 0;
  virtual int GetY() const = 0;
  virtual int GetWidth() const;
  virtual int GetHeight() const;
  virtual float GetOpacity() const = 0;

  // Whether the WebContents are too small to display the animation, in which
  // case the animation should not be shown.
  virtual bool WebContentsTooSmall(const gfx::Size& image_size) const;

  // Move the animation to wherever it should currently be.
  void Reposition();

  // Shut down the animation cleanly.
  void Close();

  // gfx::Animation
  void AnimateToState(double state) override;

  // We use a TYPE_POPUP for the popup so that it may float above any windows in
  // our UI.
  raw_ptr<views::Widget> popup_ = nullptr;

  // The content area at the start of the animation. We store this so that the
  // download shelf's resizing of the content area doesn't cause the animation
  // to move around. This means that once started, the animation won't move
  // with the parent window, but it's so fast that this shouldn't cause too
  // much heartbreak.
  gfx::Rect web_contents_bounds_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_STARTED_ANIMATION_VIEWS_H_
