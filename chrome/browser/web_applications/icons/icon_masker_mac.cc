// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/icons/icon_masker.h"

#include "base/task/bind_post_task.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/web_applications/os_integration/mac/icon_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace web_app {

namespace {

void MaskIconOnThreadPool(SkBitmap input_bitmap,
                          MaskedIconCallback final_threaded_callback) {
  gfx::Image pre_mask = gfx::Image::CreateFrom1xBitmap(input_bitmap);
  SkBitmap masked_bitmap = CreateAppleMaskedAppIcon(pre_mask).AsBitmap();
  // If masking fails for whatever reason, just return the non-masked bitmap as
  // stop-gap instead of crashing. There is a problem here that the input
  // bitmap can also be empty, but there's nothing we can do here if that is the
  // case. This also maintains consistency with how it works on Windows and
  // Linux, where the icon is "passed" through to the callback.
  if (masked_bitmap.drawsNothing()) {
    masked_bitmap = input_bitmap;
  }
  std::move(final_threaded_callback).Run(std::move(masked_bitmap));
}

}  // namespace

void MaskIconOnOs(SkBitmap input_bitmap, MaskedIconCallback masked_callback) {
  auto final_callback =
      base::BindPostTaskToCurrentDefault(std::move(masked_callback));

  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&MaskIconOnThreadPool, std::move(input_bitmap),
                     std::move(final_callback)));
}

}  // namespace web_app
