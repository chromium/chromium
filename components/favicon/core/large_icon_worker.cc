// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/large_icon_worker.h"

#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace favicon {

// The resized and decoded images generated from LargeIconService are sometimes
// (but not always) shown to the user, so the task priority was increased from
// BEST_EFFORT to USER_VISIBLE. If this potentially expensive change causes any
// issues, enable the kill switch below.
BASE_FEATURE(kLargeIconWorkerTaskPriorityKillSwitch,
             base::FEATURE_DISABLED_BY_DEFAULT);

using NoBigEnoughIconBehavior = LargeIconService::NoBigEnoughIconBehavior;

namespace {

bool ShouldReturnBitmap(const favicon_base::FaviconRawBitmapResult& db_result,
                        int min_source_size,
                        NoBigEnoughIconBehavior no_big_enough_icon_behavior) {
  return db_result.is_valid() &&
         db_result.pixel_size.width() == db_result.pixel_size.height() &&
         (db_result.pixel_size.width() >= min_source_size ||
          no_big_enough_icon_behavior ==
              NoBigEnoughIconBehavior::kReturnBitmap);
}

// Wraps the PNG data in `db_result` in a gfx::Image. If `desired_size` is not
// 0, the image gets decoded and resized to `desired_size` (in px). Must run on
// a background thread in production.
gfx::Image ResizeLargeIconOnBackgroundThread(
    const favicon_base::FaviconRawBitmapResult& db_result,
    int desired_size) {
  gfx::Image image = gfx::Image::CreateFrom1xPNGBytes(db_result.bitmap_data);

  if (desired_size == 0 || db_result.pixel_size.width() == desired_size) {
    return image;
  }

  SkBitmap resized = skia::ImageOperations::Resize(
      image.AsBitmap(), skia::ImageOperations::RESIZE_LANCZOS3, desired_size,
      desired_size);
  return gfx::Image::CreateFrom1xBitmap(resized);
}

// Returns `fallback_icon_style` or a default value picked based on
// `no_big_enough_icon_behavior`.
std::unique_ptr<favicon_base::FallbackIconStyle> FallbackIconStyleOrDefault(
    std::unique_ptr<favicon_base::FallbackIconStyle> fallback_icon_style,
    LargeIconService::NoBigEnoughIconBehavior no_big_enough_icon_behavior) {
  if (no_big_enough_icon_behavior !=
      NoBigEnoughIconBehavior::kReturnFallbackColor) {
    return nullptr;
  }
  return fallback_icon_style
             ? std::move(fallback_icon_style)
             : std::make_unique<favicon_base::FallbackIconStyle>();
}

void LogFallbackIconSizeIfNeeded(
    int favicon_width_according_to_database,
    LargeIconService::NoBigEnoughIconBehavior no_big_enough_icon_behavior) {
  if (no_big_enough_icon_behavior !=
      NoBigEnoughIconBehavior::kReturnFallbackColor) {
    return;
  }
  // The size must be positive, we cap to 128 to avoid the sparse histogram
  // to explode (having too many different values, server-side). Size 128
  // already indicates that there is a problem in the code, 128 px _should_
  // be enough in all current UI surfaces.
  DCHECK_GE(favicon_width_according_to_database, 0);
  favicon_width_according_to_database =
      std::min(favicon_width_according_to_database, 128);
  base::UmaHistogramSparse("Favicons.LargeIconService.FallbackSize",
                           favicon_width_according_to_database);
}

}  // namespace

LargeIconWorker::LargeIconWorker(
    int min_source_size_in_pixel,
    int size_in_pixel_to_resize_to,
    NoBigEnoughIconBehavior no_big_enough_icon_behavior,
    favicon_base::LargeIconCallback raw_bitmap_callback,
    favicon_base::LargeIconImageCallback image_callback,
    base::CancelableTaskTracker* tracker)
    : min_source_size_in_pixel_(min_source_size_in_pixel),
      size_in_pixel_to_resize_to_(size_in_pixel_to_resize_to),
      no_big_enough_icon_behavior_(no_big_enough_icon_behavior),
      raw_bitmap_callback_(std::move(raw_bitmap_callback)),
      image_callback_(std::move(image_callback)),
      background_task_runner_(base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(),
           base::FeatureList::IsEnabled(kLargeIconWorkerTaskPriorityKillSwitch)
               ? base::TaskPriority::BEST_EFFORT
               : base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      tracker_(tracker) {}

LargeIconWorker::~LargeIconWorker() = default;

void LargeIconWorker::OnIconLookupComplete(
    const favicon_base::FaviconRawBitmapResult& db_result) {
  icon_url_ = db_result.icon_url;
  favicon_width_according_to_database_ =
      db_result.is_valid() ? db_result.pixel_size.width() : 0;
  fallback_icon_style_ = std::make_unique<favicon_base::FallbackIconStyle>();

  if (ShouldReturnBitmap(db_result, min_source_size_in_pixel_,
                         no_big_enough_icon_behavior_)) {
    tracker_->PostTaskAndReply(
        background_task_runner_.get(), FROM_HERE,
        base::BindOnce(&LargeIconWorker::ResizeAndEncodeOnBackgroundThread,
                       this, db_result),
        base::BindOnce(&LargeIconWorker::OnIconProcessingComplete, this));
    return;
  }
  if (no_big_enough_icon_behavior_ ==
          NoBigEnoughIconBehavior::kReturnFallbackColor &&
      db_result.is_valid()) {
    tracker_->PostTaskAndReply(
        background_task_runner_.get(), FROM_HERE,
        base::BindOnce(&LargeIconWorker::ComputeDominantColorOnBackgroundThread,
                       this, db_result.bitmap_data),
        base::BindOnce(&LargeIconWorker::OnIconProcessingComplete, this));
    return;
  }

  OnIconProcessingComplete();
}

// static
base::CancelableTaskTracker::TaskId LargeIconWorker::GetLargeIconRawBitmap(
    FaviconService* favicon_service,
    const GURL& page_url,
    int min_source_size_in_pixel,
    int size_in_pixel_to_resize_to,
    NoBigEnoughIconBehavior no_big_enough_icon_behavior,
    favicon_base::LargeIconCallback raw_bitmap_callback,
    favicon_base::LargeIconImageCallback image_callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK_LE(1, min_source_size_in_pixel);
  DCHECK_LE(0, size_in_pixel_to_resize_to);

  auto worker = base::MakeRefCounted<LargeIconWorker>(
      min_source_size_in_pixel, size_in_pixel_to_resize_to,
      no_big_enough_icon_behavior, std::move(raw_bitmap_callback),
      std::move(image_callback), tracker);

  int max_size_in_pixel =
      std::max(size_in_pixel_to_resize_to, min_source_size_in_pixel);

  static const base::NoDestructor<std::vector<favicon_base::IconTypeSet>>
      large_icon_types({{favicon_base::IconType::kWebManifestIcon},
                        {favicon_base::IconType::kFavicon},
                        {favicon_base::IconType::kTouchIcon},
                        {favicon_base::IconType::kTouchPrecomposedIcon}});

  // TODO(beaudoin): For now this is just a wrapper around
  //   GetLargestRawFaviconForPageURL. Add the logic required to select the
  //   best possible large icon. Also add logic to fetch-on-demand when the
  //   URL of a large icon is known but its bitmap is not available.
  return favicon_service->GetLargestRawFaviconForPageURL(
      page_url, *large_icon_types, max_size_in_pixel,
      base::BindOnce(&LargeIconWorker::OnIconLookupComplete, worker), tracker);
}

void LargeIconWorker::ResizeAndEncodeOnBackgroundThread(
    const favicon_base::FaviconRawBitmapResult& db_result) {
  gfx::Image image =
      ResizeLargeIconOnBackgroundThread(db_result, size_in_pixel_to_resize_to_);
  if (image.IsEmpty()) {
    return;
  }

  if (raw_bitmap_callback_) {
    raw_bitmap_result_ = db_result;
    if (size_in_pixel_to_resize_to_ != 0) {
      raw_bitmap_result_.pixel_size =
          gfx::Size(size_in_pixel_to_resize_to_, size_in_pixel_to_resize_to_);
    }
    raw_bitmap_result_.bitmap_data = image.As1xPNGBytes();
  }
  if (image_callback_) {
    bitmap_result_ = image.AsBitmap();
  }
}

void LargeIconWorker::ComputeDominantColorOnBackgroundThread(
    scoped_refptr<base::RefCountedMemory> favicon_bytes) {
  favicon_base::SetDominantColorAsBackground(*favicon_bytes,
                                             fallback_icon_style_.get());
}

void LargeIconWorker::OnIconProcessingComplete() {
  // If `raw_bitmap_callback_` is provided, return the raw result.
  if (raw_bitmap_callback_) {
    if (raw_bitmap_result_.is_valid()) {
      std::move(raw_bitmap_callback_)
          .Run(favicon_base::LargeIconResult(raw_bitmap_result_));
      return;
    }
    LogFallbackIconSizeIfNeeded(favicon_width_according_to_database_,
                                no_big_enough_icon_behavior_);
    std::move(raw_bitmap_callback_)
        .Run(favicon_base::LargeIconResult(FallbackIconStyleOrDefault(
            std::move(fallback_icon_style_), no_big_enough_icon_behavior_)));
    return;
  }

  if (!bitmap_result_.isNull()) {
    std::move(image_callback_)
        .Run(favicon_base::LargeIconImageResult(
            gfx::Image::CreateFrom1xBitmap(bitmap_result_), icon_url_));
    return;
  }
  LogFallbackIconSizeIfNeeded(favicon_width_according_to_database_,
                              no_big_enough_icon_behavior_);
  std::move(image_callback_)
      .Run(favicon_base::LargeIconImageResult(FallbackIconStyleOrDefault(
          std::move(fallback_icon_style_), no_big_enough_icon_behavior_)));
}

}  // namespace favicon
