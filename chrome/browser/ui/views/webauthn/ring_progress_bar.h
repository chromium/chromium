// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_RING_PROGRESS_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_RING_PROGRESS_BAR_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"

namespace gfx {
class Animation;
class LinearAnimation;
}  // namespace gfx

// A progress bar that takes the shape of a ring.
class RingProgressBar : public views::View, public gfx::AnimationDelegate {
  METADATA_HEADER(RingProgressBar, views::View)

 public:
  RingProgressBar();
  ~RingProgressBar() override;

  // Sets the progress value, animating from |initial| to |target|. Valid values
  // are between 0 and 1.
  void SetValue(double initial, double target);

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  double initial_ = 0;
  double target_ = 0;

  std::unique_ptr<gfx::LinearAnimation> animation_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_RING_PROGRESS_BAR_H_
