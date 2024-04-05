// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_hover_card_thumbnail_observer.h"

#include "chrome/browser/ui/tabs/tab_style.h"

TabHoverCardThumbnailObserver::TabHoverCardThumbnailObserver() = default;
TabHoverCardThumbnailObserver::~TabHoverCardThumbnailObserver() = default;

void TabHoverCardThumbnailObserver::Observe(
    scoped_refptr<ThumbnailImage> thumbnail_image) {
  if (current_image_ == thumbnail_image)
    return;

  subscription_.reset();
  current_image_ = std::move(thumbnail_image);
  if (!current_image_)
    return;

  subscription_ = current_image_->Subscribe();
  if (!current_image_) {
    subscription_.reset();
    return;
  }

  subscription_->SetSizeHint(TabStyle::Get()->GetPreviewImageSize());
  subscription_->SetUncompressedImageCallback(base::BindRepeating(
      &TabHoverCardThumbnailObserver::ThumbnailImageCallback,
      base::Unretained(this), base::Unretained(current_image_.get())));

  current_image_->RequestThumbnailImage();
}

base::CallbackListSubscription TabHoverCardThumbnailObserver::AddCallback(
    Callback callback) {
  return callback_list_.Add(callback);
}

void TabHoverCardThumbnailObserver::ThumbnailImageCallback(
    const ThumbnailImage* image,
    gfx::ImageSkia preview_image) {
  if (image == current_image_.get())
    callback_list_.Notify(this, preview_image);
}
