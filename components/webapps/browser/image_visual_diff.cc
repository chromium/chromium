// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

// Calculates how different two colors are, by comparing how far apart each of
// the RGBA channels are (in terms of percentages) and averaging that across all
// channels.
float ColorPercentDiff(SkColor color1, SkColor color2) {
  float diff_red =
      std::abs(static_cast<float>(SkColorGetR(color1)) - SkColorGetR(color2)) /
      255.0f;
  float diff_green =
      std::abs(static_cast<float>(SkColorGetG(color1)) - SkColorGetG(color2)) /
      255.0f;
  float diff_blue =
      std::abs(static_cast<float>(SkColorGetB(color1)) - SkColorGetB(color2)) /
      255.0f;
  float diff_alpha =
      std::abs(static_cast<float>(SkColorGetA(color1)) - SkColorGetA(color2)) /
      255.0f;

  return (diff_red + diff_green + diff_blue + diff_alpha) / 4.0f;
}

bool HasMoreThanTenPercentImageDiffSync(SkBitmap before, SkBitmap after) {
  const bool before_empty = before.empty();
  const bool after_empty = after.empty();

  if (before_empty && after_empty) {
    return false;
  }
  if (before_empty || after_empty) {
    return true;
  }

  if (before.width() != after.width() || before.height() != after.height()) {
    return true;
  }

  if (before.colorType() != after.colorType()) {
    return true;
  }

  float difference = 0;
  float num_pixels = static_cast<float>(before.height()) * before.width();
  if (num_pixels == 0) {
    return false;
  }

  // 10% of the total pixels that form the image
  const float max_allowed_pixel_difference = 0.10f * num_pixels;

  for (int y = 0; y < before.height(); ++y) {
    for (int x = 0; x < before.width(); ++x) {
      difference +=
          ColorPercentDiff(before.getColor(x, y), after.getColor(x, y));

      if (difference >= max_allowed_pixel_difference) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace

namespace web_app {

void CheckImageDiffMoreThanTenPercent(SkBitmap before,
                                      SkBitmap after,
                                      base::OnceCallback<void(bool)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&HasMoreThanTenPercentImageDiffSync, std::move(before),
                     std::move(after)),
      std::move(callback));
}

}  // namespace web_app
