// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_LAZY_LOADING_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_LAZY_LOADING_IMAGE_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/task/cancelable_task_tracker.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace autofill {

// A wrapper for `views::ImageView` that defers loading the image content until
// the first time the view is painted and visible.
class LazyLoadingImageView : public views::View {
  METADATA_HEADER(LazyLoadingImageView, views::View)

 public:
  using ImageLoaderOnLoadSuccess =
      base::OnceCallback<void(const gfx::Image& image)>;
  using ImageLoaderOnLoadFail = base::OnceCallback<void()>;
  using ImageLoader = base::OnceCallback<void(base::CancelableTaskTracker*,
                                              ImageLoaderOnLoadSuccess,
                                              ImageLoaderOnLoadFail)>;

  LazyLoadingImageView(gfx::Size size,
                       const ui::ImageModel& placeholder,
                       ImageLoader loader);
  LazyLoadingImageView(const LazyLoadingImageView&) = delete;
  LazyLoadingImageView& operator=(const LazyLoadingImageView&) = delete;
  ~LazyLoadingImageView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  gfx::Image GetImageForTesting() const;

 private:
  void OnLoadSuccess(const gfx::Image& image);

  raw_ptr<views::ImageView> image_ = nullptr;
  ImageLoader loader_;
  bool requested_ = false;
  base::CancelableTaskTracker request_tracker_;

  base::WeakPtrFactory<LazyLoadingImageView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_LAZY_LOADING_IMAGE_VIEW_H_
