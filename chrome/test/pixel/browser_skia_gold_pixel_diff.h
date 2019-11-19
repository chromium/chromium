// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PIXEL_BROWSER_SKIA_GOLD_PIXEL_DIFF_H_
#define CHROME_TEST_PIXEL_BROWSER_SKIA_GOLD_PIXEL_DIFF_H_

#include <string>

#include "ui/base/test/skia_gold_pixel_diff.h"
#include "ui/gfx/native_widget_types.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace gfx {
class Rect;
class Image;
}  // namespace gfx

// This is the utility class for Skia Gold pixeltest.
// For an example on how to write pixeltests, please refer to the demo.
class BrowserSkiaGoldPixelDiff : public SkiaGoldPixelDiff {
 public:
  BrowserSkiaGoldPixelDiff();
  ~BrowserSkiaGoldPixelDiff() override;
  // Call Init method before using this class.
  // Args:
  // widget The instance you plan to take screenshots with.
  // screenshot_prefix The prefix for your screenshot name on GCS.
  //   For every screenshot you take, it should have a unique name
  //   across Chromium, because all screenshots (aka golden images) stores
  //   in one bucket on GCS. The standard convention is to use the browser
  //   test class name as the prefix. The name will be
  //   |screenshot_prefix| + "_" + |screenshot_name|.'
  //   E.g. 'ToolbarTest_BackButtonHover'.
  void Init(views::Widget* widget, const std::string& screenshot_prefix);

  // Take a screenshot, upload to Skia Gold and compare with the remote
  // golden image. Returns true if the screenshot is the same as the golden
  // image (compared with hashcode).
  // Args:
  // screenshot_name Make sure |screenshot_prefix| + "_" + |screenshot_name|
  //                 is unique.
  // view The view you want to take screenshot. If the screen is not what
  //      you want, you can use the other method.
  bool CompareScreenshot(const std::string& screenshot_name,
                         const views::View* view) const;

 protected:
  virtual bool GrabWindowSnapshotInternal(gfx::NativeWindow window,
                                          const gfx::Rect& snapshot_bounds,
                                          gfx::Image* image) const;

 private:
  views::Widget* widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BrowserSkiaGoldPixelDiff);
};

#endif  // CHROME_TEST_PIXEL_BROWSER_SKIA_GOLD_PIXEL_DIFF_H_
