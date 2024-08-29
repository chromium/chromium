// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_image.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/skia_conversions.h"

ThumbnailImage::Subscription::Subscription(
    scoped_refptr<ThumbnailImage> thumbnail)
    : thumbnail_(std::move(thumbnail)) {}

ThumbnailImage::Subscription::~Subscription() {
  thumbnail_->HandleSubscriptionDestroyed(this);
}

ThumbnailImage::CaptureReadiness ThumbnailImage::Delegate::GetCaptureReadiness()
    const {
  return CaptureReadiness::kNotReady;
}

ThumbnailImage::Delegate::~Delegate() {
  if (thumbnail_)
    thumbnail_->delegate_ = nullptr;
}

ThumbnailImage::ThumbnailImage(Delegate* delegate, CompressedThumbnailData data)
    : delegate_(delegate), data_(std::move(data)) {
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

ThumbnailImage::CaptureReadiness ThumbnailImage::GetCaptureReadiness() const {
  return delegate_ ? delegate_->GetCaptureReadiness()
                   : CaptureReadiness::kNotReady;
}

std::unique_ptr<ThumbnailImage::Subscription> ThumbnailImage::Subscribe() {
  // Use explicit new since Subscription constructor is private.
  auto subscription =
      base::WrapUnique(new Subscription(base::WrapRefCounted(this)));
  subscribers_.insert(subscribers_.end(), subscription.get());

  // Notify |delegate_| if this is the first subscriber.
  if (subscribers_.size() == 1)
    delegate_->ThumbnailImageBeingObservedChanged(true);

  return subscription;
}

void ThumbnailImage::AssignSkBitmap(SkBitmap bitmap,
                                    std::optional<uint64_t> frame_id) {
  thumbnail_id_ = base::Token::CreateRandom();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ThumbnailImage::CompressBitmap, std::move(bitmap),
                     frame_id),
      base::BindOnce(&ThumbnailImage::AssignJPEGData,
                     weak_ptr_factory_.GetWeakPtr(), thumbnail_id_,
                     base::TimeTicks::Now(), frame_id));
}

void ThumbnailImage::ClearData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!data_ && thumbnail_id_.is_zero())
    return;

  // If there was stored data we should notify observers that it was
  // cleared. Otherwise, a bitmap was assigned but never compressed so
  // the observers still think the thumbnail is blank.
  const bool should_notify = !!data_;

  data_.reset();
  thumbnail_id_ = base::Token();

  // Notify observers of the new, blank thumbnail.
  if (should_notify) {
    NotifyCompressedDataObservers(data_);
    NotifyUncompressedDataObservers(thumbnail_id_, gfx::ImageSkia());
  }
}

void ThumbnailImage::RequestThumbnailImage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ConvertJPEGDataToImageSkiaAndNotifyObservers();
}

void ThumbnailImage::RequestCompressedThumbnailData() {
  if (data_)
    NotifyCompressedDataObservers(data_);
}

size_t ThumbnailImage::GetCompressedDataSizeInBytes() const {
  if (!data_)
    return 0;
  return data_->data.size();
}

void ThumbnailImage::AssignJPEGData(base::Token thumbnail_id,
                                    base::TimeTicks assign_sk_bitmap_time,
                                    std::optional<uint64_t> frame_id_for_trace,
                                    std::vector<uint8_t> data) {
  // If the image is stale (a new thumbnail was assigned or the
  // thumbnail was cleared after AssignSkBitmap), ignore it.
  if (thumbnail_id != thumbnail_id_) {
    if (async_operation_finished_callback_)
      async_operation_finished_callback_.Run();
    return;
  }

  data_ = base::MakeRefCounted<base::RefCountedData<std::vector<uint8_t>>>(
      std::move(data));

  // We select a TRACE_EVENT_* macro based on |frame_id|'s presence.
  // Since these are scoped traces, the macro invocation must be in the
  // enclosing scope of these operations. Extract them into a common
  // function.
  auto notify = [&]() {
    NotifyCompressedDataObservers(data_);
    ConvertJPEGDataToImageSkiaAndNotifyObservers();
  };

  if (frame_id_for_trace) {
    TRACE_EVENT_WITH_FLOW0("ui", "Tab.Preview.JPEGReceivedOnUIThreadWithFlow",
                           *frame_id_for_trace, TRACE_EVENT_FLAG_FLOW_IN);
    notify();
  } else {
    TRACE_EVENT0("ui", "Tab.Preview.JPEGReceivedOnUIThread");
    notify();
  }
}

bool ThumbnailImage::ConvertJPEGDataToImageSkiaAndNotifyObservers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
                     weak_ptr_factory_.GetWeakPtr(), thumbnail_id_));
}

void ThumbnailImage::NotifyUncompressedDataObservers(base::Token thumbnail_id,
                                                     gfx::ImageSkia image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (async_operation_finished_callback_)
    async_operation_finished_callback_.Run();

  // If the image is stale (a new thumbnail was assigned or the
  // thumbnail was cleared after AssignSkBitmap), ignore it.
  if (thumbnail_id != thumbnail_id_)
    return;

  for (Subscription* subscription : subscribers_) {
    auto size_hint = subscription->size_hint_;
    if (subscription->uncompressed_image_callback_) {
      auto cropped_image = size_hint && !image.isNull()
                               ? CropPreviewImage(image, *size_hint)
                               : image;
      subscription->uncompressed_image_callback_.Run(cropped_image);
    }
  }
}

void ThumbnailImage::NotifyCompressedDataObservers(
    CompressedThumbnailData data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Subscription* subscription : subscribers_) {
    if (subscription->compressed_image_callback_)
      subscription->compressed_image_callback_.Run(data);
  }
}

// static
std::vector<uint8_t> ThumbnailImage::CompressBitmap(
    SkBitmap bitmap,
    std::optional<uint64_t> frame_id) {
  constexpr int kCompressionQuality = 97;
  std::vector<uint8_t> data;

  // Similar to above, extract logic into function so we can select a
  // TRACE_EVENT_* macro.
  auto compress = [&]() {
    const bool result =
        gfx::JPEGCodec::Encode(bitmap, kCompressionQuality, &data);
    DCHECK(result);
  };

  if (frame_id) {
    TRACE_EVENT_WITH_FLOW0(
        "ui", "Tab.Preview.CompressJPEGWithFlow", *frame_id,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
    compress();
  } else {
    TRACE_EVENT0("ui", "Tab.Preview.CompressJPEG");
    compress();
  }

  return data;
}

// static
gfx::ImageSkia ThumbnailImage::UncompressImage(
    CompressedThumbnailData compressed) {
  gfx::ImageSkia result;
  std::unique_ptr<SkBitmap> bitmap(
      gfx::JPEGCodec::Decode(compressed->data.data(), compressed->data.size()));
  if (bitmap.get()) {
    result = gfx::ImageSkia::CreateFrom1xBitmap(*bitmap);
  }

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
      static_cast<float>(minimum_size.width()) / minimum_size.height();
  const float source_aspect =
      static_cast<float>(source_image.width()) / source_image.height();

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

void ThumbnailImage::HandleSubscriptionDestroyed(Subscription* subscription) {
  // The order of |subscribers_| does not matter. We can simply swap
  // |subscription| in |subscribers_| with the last element, then pop it
  // off the back.
  auto it = base::ranges::find(subscribers_, subscription);
  CHECK(it != subscribers_.end(), base::NotFatalUntil::M130);
  std::swap(*it, *(subscribers_.end() - 1));
  subscribers_.pop_back();

  // If that was the last subscriber, tell |delegate_| (if it still exists).
  if (delegate_ && subscribers_.empty())
    delegate_->ThumbnailImageBeingObservedChanged(false);
}
