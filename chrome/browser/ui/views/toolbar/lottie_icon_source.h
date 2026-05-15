// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_LOTTIE_ICON_SOURCE_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_LOTTIE_ICON_SOURCE_H_

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/lottie/animation.h"

namespace gfx {
class Canvas;
}

// A CanvasImageSource that draws a specific frame of a Lottie animation at a
// given size and color.
class LottieIconSource : public gfx::CanvasImageSource {
 public:
  LottieIconSource(lottie::Animation* animation,
                   float progress,
                   int size,
                   SkColor color);
  LottieIconSource(const LottieIconSource&) = delete;
  LottieIconSource& operator=(const LottieIconSource&) = delete;
  ~LottieIconSource() override;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

 private:
  const raw_ptr<lottie::Animation> animation_;
  const float progress_;
  const SkColor color_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_LOTTIE_ICON_SOURCE_H_
