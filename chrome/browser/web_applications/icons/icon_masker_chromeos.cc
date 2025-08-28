// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/icons/icon_masker.h"

#include <map>
#include <utility>

#include "base/task/bind_post_task.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/grit/app_icon_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace web_app {

namespace {

void MaskIconWithObtainedMask(SkBitmap input_bitmap,
                              SkBitmap mask_bitmap,
                              MaskedIconCallback final_threaded_callback) {
  gfx::ImageSkia input_image = gfx::ImageSkia::CreateFrom1xBitmap(input_bitmap);
  int icon_size = input_image.width();

  CHECK(!mask_bitmap.drawsNothing());
  gfx::ImageSkia mask_image;
  mask_image.AddRepresentation(gfx::ImageSkiaRep(
      skia::ImageOperations::Resize(mask_bitmap,
                                    skia::ImageOperations::RESIZE_LANCZOS3,
                                    icon_size, icon_size),
      /*scale=*/1.0f));

  gfx::ImageSkia result_image =
      gfx::ImageSkiaOperations::CreateButtonBackground(SK_ColorWHITE,
                                                       input_image, mask_image);
  std::move(final_threaded_callback).Run(*result_image.bitmap());
}

}  // namespace

void MaskIconOnOs(SkBitmap input_bitmap, MaskedIconCallback masked_callback) {
  const SkBitmap* mask_bitmap = ui::ResourceBundle::GetSharedInstance()
                                    .GetImageNamed(IDR_ICON_MASK)
                                    .ToSkBitmap();

  auto final_callback =
      base::BindPostTaskToCurrentDefault(std::move(masked_callback));

  // Perform masking asynchronously.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&MaskIconWithObtainedMask, std::move(input_bitmap),
                     *mask_bitmap, std::move(final_callback)));
}

}  // namespace web_app
