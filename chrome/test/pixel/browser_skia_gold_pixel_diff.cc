// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/pixel/browser_skia_gold_pixel_diff.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/snapshot/snapshot_aura.h"
#endif

void SnapshotCallback(base::RunLoop* run_loop,
                      gfx::Image* ret_image,
                      gfx::Image image) {
  *ret_image = image;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop->QuitClosure());
}

BrowserSkiaGoldPixelDiff::BrowserSkiaGoldPixelDiff() = default;

BrowserSkiaGoldPixelDiff::~BrowserSkiaGoldPixelDiff() = default;

void BrowserSkiaGoldPixelDiff::Init(views::Widget* widget,
                                    const std::string& screenshot_prefix) {
  SkiaGoldPixelDiff::Init(screenshot_prefix);
  DCHECK(widget);
  widget_ = widget;
}

bool BrowserSkiaGoldPixelDiff::GrabWindowSnapshotInternal(
    gfx::NativeWindow window,
    const gfx::Rect& snapshot_bounds,
    gfx::Image* image) const {
  base::RunLoop run_loop;
#if defined(USE_AURA)
  ui::GrabWindowSnapshotAsyncAura(
#else
  ui::GrabWindowSnapshotAsync(
#endif
      window, snapshot_bounds,
      base::BindOnce(&SnapshotCallback, &run_loop, image));
  run_loop.Run();
  return !image->IsEmpty();
}

bool BrowserSkiaGoldPixelDiff::CompareScreenshot(
    const std::string& screenshot_name,
    const views::View* view) const {
  DCHECK(Initialized()) << "Initialize the class before using this method.";
  gfx::Rect rc = view->GetBoundsInScreen();
  gfx::Rect bounds_in_screen = widget_->GetRootView()->GetBoundsInScreen();
  gfx::Rect bounds = widget_->GetRootView()->bounds();
  rc.Offset(bounds.x() - bounds_in_screen.x(),
            bounds.y() - bounds_in_screen.y());
  gfx::Image image;
  bool ret = GrabWindowSnapshotInternal(widget_->GetNativeWindow(), rc, &image);
  if (!ret) {
    LOG(ERROR) << "Grab screenshot failed.";
    return false;
  }
  return SkiaGoldPixelDiff::CompareScreenshot(screenshot_name,
                                              *image.ToSkBitmap());
}
