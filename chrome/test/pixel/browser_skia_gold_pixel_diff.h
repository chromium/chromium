// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PIXEL_BROWSER_SKIA_GOLD_PIXEL_DIFF_H_
#define CHROME_TEST_PIXEL_BROWSER_SKIA_GOLD_PIXEL_DIFF_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/test/skia_gold_pixel_diff.h"
#include "ui/gfx/native_widget_types.h"

namespace views {
class View;
}  // namespace views

namespace gfx {
class Rect;
class Image;
}  // namespace gfx

namespace ui {
namespace test {
class SkiaGoldMatchingAlgorithm;
class SkiaGoldPixelDiff;
}  // namespace test
}  // namespace ui

// This is the utility class for Skia Gold pixeltest.
// For an example on how to write pixeltests, please refer to the demo.
// NOTE: this class has to be initialized before using. A screenshot prefix and
// a corpus string are required for initialization. Check
// `SkiaGoldPixelDiff::Init()` for more details.
class BrowserSkiaGoldPixelDiff : public ui::test::SkiaGoldPixelDiff {
 public:
  BrowserSkiaGoldPixelDiff();

  BrowserSkiaGoldPixelDiff(const BrowserSkiaGoldPixelDiff&) = delete;
  BrowserSkiaGoldPixelDiff& operator=(const BrowserSkiaGoldPixelDiff&) = delete;

  ~BrowserSkiaGoldPixelDiff() override;

  // Takes a screenshot then uploads to Skia Gold and compares it with the
  // remote golden image. Returns true if the screenshot is the same as the
  // golden image (compared with hashcode).
  // `screenshot_name` specifies the name of the screenshot to be taken. For
  // every screenshot you take, it should have a unique name across Chromium,
  // because all screenshots (aka golden images) stores in one bucket on GCS.
  // The standard convention is to use the browser test class name as the
  // prefix. The name will be `screenshot_prefix` + "_" + `screenshot_name`.
  // E.g. 'ToolbarTest_BackButtonHover'. Here `screenshot_prefix` is passed as
  // an argument during initialization.
  // `view` is the view you want to take screenshot.
  bool CompareScreenshot(
      const std::string& screenshot_name,
      views::View* view,
      const ui::test::SkiaGoldMatchingAlgorithm* algorithm = nullptr) const;

 protected:
  virtual bool GrabWindowSnapshotInternal(gfx::NativeWindow window,
                                          const gfx::Rect& snapshot_bounds,
                                          gfx::Image* image) const;
};

#endif  // CHROME_TEST_PIXEL_BROWSER_SKIA_GOLD_PIXEL_DIFF_H_
