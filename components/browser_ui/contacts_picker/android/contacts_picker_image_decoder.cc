// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/contacts_picker/android/contacts_picker_image_decoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "skia/ext/image_operations.h"
#include "third_party/jni_zero/jni_zero.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/size.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/contacts_picker/android/contacts_picker_jni_headers/ContactsPickerImageDecoder_jni.h"

namespace browser_ui {

namespace {

// Cap encoded input at 1 MiB (matching the Java cursor threshold).
constexpr size_t kMaxEncodedBytes = 1024 * 1024;

// Cap decoded avatar bitmap at 4 MiB (e.g. 1024x1024 @ 4bpp) to prevent
// decompression bomb memory exhaustion in the browser process.
constexpr uint64_t kMaxDecodedImageBytes = 4 * 1024 * 1024;

// Sanity upper bound on icon dimension to prevent integer overflow.
constexpr int32_t kMaxAvatarDimension = 1024;

data_decoder::DataDecoder* GetDataDecoder() {
  DCHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
  static base::NoDestructor<data_decoder::DataDecoder> data_decoder;
  return data_decoder.get();
}

// Scales `bitmap` to the desired square size and converts it to a Java Bitmap
// on a ThreadPool worker thread to prevent UI jank.
base::android::ScopedJavaGlobalRef<jobject> ScaleAndConvertBitmapInBackground(
    const SkBitmap& bitmap,
    int32_t desired_size) {
  SkBitmap scaled_bitmap = ScaleContactIconBitmap(bitmap, desired_size);
  if (scaled_bitmap.drawsNothing() || scaled_bitmap.isNull()) {
    return {};
  }

  base::android::ScopedJavaLocalRef<jobject> j_bitmap =
      gfx::ConvertToJavaBitmap(scaled_bitmap,
                               gfx::OomBehavior::kReturnNullOnOom);
  if (!j_bitmap) {
    return {};
  }

  return base::android::ScopedJavaGlobalRef<jobject>(j_bitmap);
}

void OnBitmapConverted(base::android::ScopedJavaGlobalRef<jobject> j_callback,
                       base::android::ScopedJavaGlobalRef<jobject> j_bitmap) {
  DCHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
  base::android::RunObjectCallbackAndroid(j_callback, j_bitmap);
}

void OnImageDecoded(base::android::ScopedJavaGlobalRef<jobject> j_callback,
                    int32_t desired_size,
                    const SkBitmap& bitmap) {
  DCHECK(base::SingleThreadTaskRunner::HasCurrentDefault());

  if (bitmap.drawsNothing() || bitmap.isNull()) {
    base::android::RunObjectCallbackAndroid(j_callback, nullptr);
    return;
  }

  // Offload resizing and Java Bitmap conversion to ThreadPool.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ScaleAndConvertBitmapInBackground, bitmap, desired_size),
      base::BindOnce(&OnBitmapConverted, std::move(j_callback)));
}

}  // namespace

SkBitmap ScaleContactIconBitmap(const SkBitmap& bitmap, int32_t desired_size) {
  if (bitmap.drawsNothing() || bitmap.isNull()) {
    return {};
  }

  // `data_decoder::DecodeImage` returns `skia.mojom.BitmapN32`, which
  // guarantees `kN32_SkColorType` (required by `skia::ImageOperations::Resize`
  // and `gfx::ConvertToJavaBitmap`).
  CHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  int orig_w = bitmap.width();
  int orig_h = bitmap.height();
  if (orig_w <= 0 || orig_h <= 0) {
    return {};
  }

  if (desired_size > 0 && (orig_w != desired_size || orig_h != desired_size)) {
    return skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_BEST, desired_size, desired_size);
  }
  return bitmap;
}

static void JNI_ContactsPickerImageDecoder_DecodeImage(
    JNIEnv* env,
    const base::android::JavaRef<jbyteArray>& j_data,
    int32_t desired_size,
    const base::android::JavaRef<jobject>& j_callback) {
  DCHECK(base::SingleThreadTaskRunner::HasCurrentDefault());

  if (!j_data) {
    base::android::RunObjectCallbackAndroid(j_callback, nullptr);
    return;
  }

  size_t data_size = j_data.GetSize(env);
  if (data_size == 0 || data_size > kMaxEncodedBytes) {
    base::android::RunObjectCallbackAndroid(j_callback, nullptr);
    return;
  }

  int32_t clamped_size = std::clamp(desired_size, 0, kMaxAvatarDimension);
  gfx::Size desired_frame_size =
      clamped_size > 0 ? gfx::Size(clamped_size, clamped_size) : gfx::Size();

  auto decode_callback = base::BindOnce(
      &OnImageDecoded,
      base::android::ScopedJavaGlobalRef<jobject>(env, j_callback),
      clamped_size);

  std::vector<uint8_t> buffer(data_size);
  base::android::JavaByteArrayToByteSpan(env, j_data, buffer);
  data_decoder::DecodeImage(GetDataDecoder(), buffer,
                            data_decoder::mojom::ImageCodec::kDefault,
                            /*shrink_to_fit=*/true, kMaxDecodedImageBytes,
                            desired_frame_size, std::move(decode_callback));
}

}  // namespace browser_ui

DEFINE_JNI(ContactsPickerImageDecoder)
