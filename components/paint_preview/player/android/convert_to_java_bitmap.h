// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_CONVERT_TO_JAVA_BITMAP_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_CONVERT_TO_JAVA_BITMAP_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"

class SkBitmap;

namespace paint_preview {

// Holder class for a Java bitmap.
struct JavaBitmapResult {
  JavaBitmapResult(mojom::PaintPreviewCompositor::BitmapStatus status,
                   size_t bytes,
                   base::android::ScopedJavaGlobalRef<jobject> java_bitmap);
  ~JavaBitmapResult();

  JavaBitmapResult& operator=(JavaBitmapResult&& other) noexcept;
  JavaBitmapResult(JavaBitmapResult&& other) noexcept;

  mojom::PaintPreviewCompositor::BitmapStatus status;
  size_t bytes;
  base::android::ScopedJavaGlobalRef<jobject> java_bitmap;
};

// Converts `sk_bitmap` into a Java bitmap if possible and forwards along
// status.
void ConvertToJavaBitmap(base::OnceCallback<void(JavaBitmapResult)> callback,
                         mojom::PaintPreviewCompositor::BitmapStatus status,
                         const SkBitmap& sk_bitmap);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_ANDROID_CONVERT_TO_JAVA_BITMAP_H_
