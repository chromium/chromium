// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_image.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/skia_util.h"

void ThumbnailImage::Observer::OnThumbnailImageAvailable(
    gfx::ImageSkia thumbnail_image) {}

void ThumbnailImage::Observer::OnCompressedThumbnailDataAvailable(
    CompressedThumbnailData thumbnail_data) {}

base::Optional<gfx::Size> ThumbnailImage::Observer::GetThumbnailSizeHint()
    const {
  return base::nullopt;
}

ThumbnailImage::Delegate::~Delegate() {
  if (thumbnail_)
    thumbnail_->delegate_ = nullptr;
}

ThumbnailImage::ThumbnailImage(Delegate* delegate) : delegate_(delegate) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  DCHECK(!delegate_->thumbnail_);
  delegate_->thumbnail_ = this;
}

ThumbnailImage::~ThumbnailImage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegate_)
    delegate_->thumbnail_ = nullptr;
}

void ThumbnailImage::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  if (!observers_.HasObserver(observer)) {
    const bool is_first_observer = !observers_.might_have_observers();
    observers_.AddObserver(observer);
    if (is_first_observer && delegate_)
      delegate_->ThumbnailImageBeingObservedChanged(true);
  }
}

void ThumbnailImage::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  if (observers_.HasObserver(observer)) {
    observers_.RemoveObserver(observer);
    if (delegate_ && !observers_.might_have_observers())
      delegate_->ThumbnailImageBeingObservedChanged(false);
  }
}

bool ThumbnailImage::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void ThumbnailImage::AssignSkBitmap(SkBitmap bitmap) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailImage::CompressBitmap, std::move(bitmap)),
      base::BindOnce(&ThumbnailImage::AssignJPEGData,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ThumbnailImage::RequestThumbnailImage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ConvertJPEGDataToImageSkiaAndNotifyObservers();
}

void ThumbnailImage::RequestCompressedThumbnailData() {
  if (data_)
    NotifyCompressedDataObservers(data_);
}

void ThumbnailImage::AssignJPEGData(base::TimeTicks assign_sk_bitmap_time,
                                    std::vector<uint8_t> data) {
  data_ = base::MakeRefCounted<base::RefCountedData<std::vector<uint8_t>>>(
      std::move(data));
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Tab.Preview.TimeToNotifyObserversAfterCaptureReceived",
      base::TimeTicks::Now() - assign_sk_bitmap_time,
      base::TimeDelta::FromMicroseconds(100),
      base::TimeDelta::FromMilliseconds(100), 50);
  NotifyCompressedDataObservers(data_);
  ConvertJPEGDataToImageSkiaAndNotifyObservers();
}

bool ThumbnailImage::ConvertJPEGDataToImageSkiaAndNotifyObservers() {
  if (!data_) {
    if (async_operation_finished_callback_)
      async_operation_finished_callback_.Run();
    return false;
  }
  return base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailImage::UncompressImage, data_),
      base::BindOnce(&ThumbnailImage::NotifyUncompressedDataObservers,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ThumbnailImage::NotifyUncompressedDataObservers(gfx::ImageSkia image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (async_operation_finished_callback_)
    async_operation_finished_callback_.Run();
  for (auto& observer : observers_) {
    auto size_hint = observer.GetThumbnailSizeHint();
    observer.OnThumbnailImageAvailable(
        size_hint ? CropPreviewImage(image, *size_hint) : image);
  }
}

void ThumbnailImage::NotifyCompressedDataObservers(
    CompressedThumbnailData data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers_)
    observer.OnCompressedThumbnailDataAvailable(data);
}

// static
std::vector<uint8_t> ThumbnailImage::CompressBitmap(SkBitmap bitmap) {
  constexpr int kCompressionQuality = 97;
  std::vector<uint8_t> data;
  const bool result =
      gfx::JPEGCodec::Encode(bitmap, kCompressionQuality, &data);
  DCHECK(result);
  return data;
}

// static
gfx::ImageSkia ThumbnailImage::UncompressImage(
    CompressedThumbnailData compressed) {
  gfx::ImageSkia result =
      gfx::ImageSkia::CreateFrom1xBitmap(*gfx::JPEGCodec::Decode(
          compressed->data.data(), compressed->data.size()));
  result.MakeThreadSafe();
  return result;
}

// static
gfx::ImageSkia ThumbnailImage::CropPreviewImage(
    const gfx::ImageSkia& source_image,
    const gfx::Size& minimum_size) {
  DCHECK(!source_image.isNull());
  DCHECK(!source_image.size().IsEmpty());
  DCHECK(!minimum_size.IsEmpty());
  const float desired_aspect =
      float{minimum_size.width()} / minimum_size.height();
  const float source_aspect =
      float{source_image.width()} / float{source_image.height()};

  if (source_aspect == desired_aspect ||
      source_image.width() < minimum_size.width() ||
      source_image.height() < minimum_size.height()) {
    return source_image;
  }

  gfx::Rect clip_rect;
  if (source_aspect > desired_aspect) {
    // Wider than tall, clip horizontally: we center the smaller
    // thumbnail in the wider screen.
    const int new_width = source_image.height() * desired_aspect;
    const int x_offset = (source_image.width() - new_width) / 2;
    clip_rect = {x_offset, 0, new_width, source_image.height()};
  } else {
    // Taller than wide; clip vertically.
    const int new_height = source_image.width() / desired_aspect;
    clip_rect = {0, 0, source_image.width(), new_height};
  }

  SkBitmap cropped;
  source_image.bitmap()->extractSubset(&cropped, gfx::RectToSkIRect(clip_rect));
  return gfx::ImageSkia::CreateFrom1xBitmap(cropped);
}
