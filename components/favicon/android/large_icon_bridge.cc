// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/android/large_icon_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "components/favicon/android/jni_headers/LargeIconBridge_jni.h"
#include "components/favicon/content/large_favicon_provider_getter.h"
#include "components/favicon/core/core_favicon_service.h"
#include "components/favicon/core/large_favicon_provider.h"
#include "components/favicon/core/large_icon_worker.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace favicon {

namespace {

void OnLargeIconAvailable(const JavaRef<jobject>& j_callback,
                          const favicon_base::LargeIconResult& result) {
  JNIEnv* env = AttachCurrentThread();

  // Convert the result to a Java Bitmap.
  SkBitmap bitmap;
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (result.bitmap.is_valid()) {
    if (gfx::PNGCodec::Decode(result.bitmap.bitmap_data->front(),
                              result.bitmap.bitmap_data->size(), &bitmap))
      j_bitmap = gfx::ConvertToJavaBitmap(bitmap);
  }

  favicon_base::FallbackIconStyle fallback;
  if (result.fallback_icon_style)
    fallback = *result.fallback_icon_style;

  Java_LargeIconCallback_onLargeIconAvailable(
      env, j_callback, j_bitmap, fallback.background_color,
      fallback.is_default_background_color,
      static_cast<int>(result.bitmap.icon_type));
}

}  // namespace

static jlong JNI_LargeIconBridge_Init(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new LargeIconBridge());
}

LargeIconBridge::LargeIconBridge() = default;
LargeIconBridge::~LargeIconBridge() = default;

void LargeIconBridge::Destroy(JNIEnv* env) {
  delete this;
}

jboolean LargeIconBridge::GetLargeIconForURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_browser_context,
    const JavaParamRef<jobject>& j_page_url,
    jint min_source_size_px,
    const JavaParamRef<jobject>& j_callback) {
  content::BrowserContext* browser_context =
      content::BrowserContextFromJavaHandle(j_browser_context);
  if (!browser_context)
    return false;

  LargeFaviconProvider* favicon_provider =
      GetLargeFaviconProvider(browser_context);
  if (!favicon_provider)
    return false;

  favicon_base::LargeIconCallback callback_runner = base::BindOnce(
      &OnLargeIconAvailable, ScopedJavaGlobalRef<jobject>(env, j_callback));

  std::unique_ptr<GURL> url = url::GURLAndroid::ToNativeGURL(env, j_page_url);

  // Use desired_size = 0 for getting the icon from the cache (so that
  // the icon is not poorly rescaled by LargeIconService).
  LargeIconWorker::GetLargeIconRawBitmap(
      favicon_provider, *url, min_source_size_px,
      /*desired_size_in_pixel=*/0, std::move(callback_runner), {},
      &cancelable_task_tracker_);

  return true;
}

}  // namespace favicon
