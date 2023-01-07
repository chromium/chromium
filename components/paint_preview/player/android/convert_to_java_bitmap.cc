// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/android/convert_to_java_bitmap.h"

#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"

namespace paint_preview {

JavaBitmapResult::JavaBitmapResult(
    mojom::PaintPreviewCompositor::BitmapStatus status,
    size_t bytes,
    base::android::ScopedJavaGlobalRef<jobject> java_bitmap)
    : status(status), bytes(bytes), java_bitmap(java_bitmap) {}

JavaBitmapResult::~JavaBitmapResult() = default;

JavaBitmapResult& JavaBitmapResult::operator=(
    JavaBitmapResult&& other) noexcept = default;

JavaBitmapResult::JavaBitmapResult(JavaBitmapResult&& other) noexcept = default;

void ConvertToJavaBitmap(base::OnceCallback<void(JavaBitmapResult)> callback,
                         mojom::PaintPreviewCompositor::BitmapStatus status,
                         const SkBitmap& sk_bitmap) {
  TRACE_EVENT0("paint_preview", "ConvertToJavaBitmap");
  if (status != mojom::PaintPreviewCompositor::BitmapStatus::kSuccess) {
    std::move(callback).Run(JavaBitmapResult(status, 0U, nullptr));
    return;
  }

  if (sk_bitmap.isNull() || sk_bitmap.info().width() <= 0 ||
      sk_bitmap.info().height() <= 0) {
    // Assume allocation failure.
    std::move(callback).Run(JavaBitmapResult(
        mojom::PaintPreviewCompositor::BitmapStatus::kAllocFailed, 0U,
        nullptr));
    return;
  }

  std::move(callback).Run(JavaBitmapResult(
      status, sk_bitmap.computeByteSize(),
      base::android::ScopedJavaGlobalRef<jobject>(gfx::ConvertToJavaBitmap(
          sk_bitmap, gfx::OomBehavior::kReturnNullOnOom))));
}

}  // namespace paint_preview
