// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/lazy_loading_image_view.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"

namespace autofill {
LazyLoadingImageView::LazyLoadingImageView(gfx::Size size,
                                           const ui::ImageModel& placeholder,
                                           ImageLoader loader)
    : loader_(std::move(loader)) {
  SetUseDefaultFillLayout(true);

  image_ = AddChildView(views::Builder<views::ImageView>()
                            .SetImageSize(size)
                            .SetImage(placeholder)
                            .Build());
}
LazyLoadingImageView::~LazyLoadingImageView() = default;

void LazyLoadingImageView::OnLoadSuccess(const gfx::Image& image) {
  image_->SetImage(image.AsImageSkia());
}

void LazyLoadingImageView::OnPaint(gfx::Canvas* canvas) {
  if (!requested_ && !GetVisibleBounds().IsEmpty()) {
    CHECK(loader_);
    std::move(loader_).Run(&request_tracker_,
                           base::BindOnce(&LazyLoadingImageView::OnLoadSuccess,
                                          weak_ptr_factory_.GetWeakPtr()),
                           // Error loading is currently ignored, which means
                           // the placeholder image doesn't change.
                           base::DoNothing());

    requested_ = true;
  }

  views::View::OnPaint(canvas);
}

gfx::Image LazyLoadingImageView::GetImageForTesting() const {
  return gfx::Image(image_->GetImage());
}

BEGIN_METADATA(LazyLoadingImageView)
END_METADATA

}  // namespace autofill
