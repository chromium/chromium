// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_TUCKER_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_TUCKER_H_

#include "base/memory/raw_ref.h"
#include "ui/gfx/geometry/point.h"

namespace views {
class Widget;
}  // namespace views

class PictureInPictureBoundsChangeAnimation;

// The PictureInPictureTucker temporarily moves a picture-in-picture widget
// mostly offscreen ("tuck"ed away) so the user can get more screen real estate
// for another task.
class PictureInPictureTucker {
 public:
  // `pip_widget` is the picture-in-picture Widget to be tucked/untucked.
  // `pip_widget` must outlive `this`.
  explicit PictureInPictureTucker(views::Widget& pip_widget);
  PictureInPictureTucker(const PictureInPictureTucker&) = delete;
  PictureInPictureTucker& operator=(const PictureInPictureTucker&) = delete;
  ~PictureInPictureTucker();

  // Moves `pip_widget_` mostly offscreen. If `pip_widget_` is already tucked,
  // then it is tucked again but the place it untucks to is not updated. This
  // allows the caller to retuck for a size adjustment without losing the
  // original location.
  void Tuck();

  // Moves `pip_widget_` back to where it was when `Tuck()` was called.
  void Untuck();

  // Forces `animation_` to complete. Used in tests to check final state.
  void FinishAnimationForTesting();

 private:
  raw_ref<views::Widget> pip_widget_;
  bool tucking_ = false;
  gfx::Point pretuck_location_;
  std::unique_ptr<PictureInPictureBoundsChangeAnimation> animation_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_TUCKER_H_
