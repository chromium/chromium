// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapps_icon_utils.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/color_analysis.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/webapps_jni_headers/WebappsIconUtils_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace webapps {

namespace {

int g_ideal_homescreen_icon_size = -1;
int g_minimum_homescreen_icon_size = -1;
int g_ideal_splash_image_size = -1;
int g_minimum_splash_image_size = -1;
int g_ideal_monochrome_icon_size = -1;
int g_ideal_adaptive_launcher_icon_size = -1;
int g_ideal_shortcut_icon_size = -1;

// Retrieves and caches the ideal and minimum sizes of the Home screen icon,
// the splash screen image, and the shortcut icons.
void GetIconSizes() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jintArray> java_size_array =
      Java_WebappsIconUtils_getIconSizes(env);
  std::vector<int> sizes;
  base::android::JavaIntArrayToIntVector(env, java_size_array, &sizes);

  // Check that the size returned is what is expected.
  DCHECK_EQ(7u, sizes.size());

  // This ordering must be kept up to date with the Java WebappsIconUtils.
  g_ideal_homescreen_icon_size = sizes[0];
  g_minimum_homescreen_icon_size = sizes[1];
  g_ideal_splash_image_size = sizes[2];
  g_minimum_splash_image_size = sizes[3];
  g_ideal_monochrome_icon_size = sizes[4];
  g_ideal_adaptive_launcher_icon_size = sizes[5];
  g_ideal_shortcut_icon_size = sizes[6];

  // Try to ensure that the data returned is sensible.
  DCHECK_LE(g_minimum_homescreen_icon_size, g_ideal_homescreen_icon_size);
  DCHECK_LE(g_minimum_splash_image_size, g_ideal_splash_image_size);
}

}  // anonymous namespace

int WebappsIconUtils::GetIdealHomescreenIconSizeInPx() {
  if (g_ideal_homescreen_icon_size == -1)
    GetIconSizes();
  return g_ideal_homescreen_icon_size;
}

int WebappsIconUtils::GetMinimumHomescreenIconSizeInPx() {
  if (g_minimum_homescreen_icon_size == -1)
    GetIconSizes();
  return g_minimum_homescreen_icon_size;
}

int WebappsIconUtils::GetIdealSplashImageSizeInPx() {
  if (g_ideal_splash_image_size == -1)
    GetIconSizes();
  return g_ideal_splash_image_size;
}

int WebappsIconUtils::GetMinimumSplashImageSizeInPx() {
  if (g_minimum_splash_image_size == -1)
    GetIconSizes();
  return g_minimum_splash_image_size;
}

int WebappsIconUtils::GetIdealAdaptiveLauncherIconSizeInPx() {
  if (g_ideal_adaptive_launcher_icon_size == -1)
    GetIconSizes();
  return g_ideal_adaptive_launcher_icon_size;
}

int WebappsIconUtils::GetIdealShortcutIconSizeInPx() {
  if (g_ideal_shortcut_icon_size == -1)
    GetIconSizes();
  return g_ideal_shortcut_icon_size;
}

int WebappsIconUtils::GetIdealIconSizeForIconType(
    webapk::Image::Usage usage,
    webapk::Image::Purpose purpose) {
  switch (usage) {
    case webapk::Image::PRIMARY_ICON:
      if (purpose == webapk::Image::MASKABLE) {
        return GetIdealAdaptiveLauncherIconSizeInPx();
      } else {
        return GetIdealHomescreenIconSizeInPx();
      }
    case webapk::Image::SPLASH_ICON:
      return GetIdealSplashImageSizeInPx();
    case webapk::Image::SHORTCUT_ICON:
      return GetIdealShortcutIconSizeInPx();
    default:
      return 0;
  }
}

bool WebappsIconUtils::DoesAndroidSupportMaskableIcons() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_OREO;
}

void WebappsIconUtils::FinalizeLauncherIconInBackground(
    const SkBitmap& bitmap,
    const GURL& url,
    scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
    base::OnceCallback<void(const SkBitmap&, bool)> callback) {
  base::AssertLongCPUWorkAllowed();

  JNIEnv* env = base::android::AttachCurrentThread();

  if (!bitmap.isNull() && Java_WebappsIconUtils_isIconLargeEnoughForLauncher(
                              env, bitmap.width(), bitmap.height())) {
    ui_thread_task_runner->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), bitmap, false));
    return;
  }

  ScopedJavaLocalRef<jobject> java_url =
      url::GURLAndroid::FromNativeGURL(env, url);
  SkColor mean_color = SkColorSetRGB(0x91, 0x91, 0x91);

  if (!bitmap.isNull()) {
    mean_color = color_utils::CalculateKMeanColorOfBitmap(bitmap);
  }

  ScopedJavaLocalRef<jobject> result =
      Java_WebappsIconUtils_generateHomeScreenIcon(
          env, java_url, SkColorGetR(mean_color), SkColorGetG(mean_color),
          SkColorGetB(mean_color));

  SkBitmap result_bitmap =
      result.obj() ? gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(result))
                   : SkBitmap();
  ui_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result_bitmap, true));
}

SkBitmap WebappsIconUtils::GenerateHomeScreenIconInBackground(
    const GURL& page_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_url =
      url::GURLAndroid::FromNativeGURL(env, page_url);
  ScopedJavaLocalRef<jobject> result =
      Java_WebappsIconUtils_generateHomeScreenIcon(env, java_url, /*red=*/0x91,
                                                   /*green=*/0x91,
                                                   /*blue=*/0x91);
  SkBitmap result_bitmap =
      result.obj() ? gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(result))
                   : SkBitmap();
  return result_bitmap;
}

SkBitmap WebappsIconUtils::GenerateAdaptiveIconBitmap(const SkBitmap& bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result;

  if (!bitmap.isNull()) {
    ScopedJavaLocalRef<jobject> java_bitmap = gfx::ConvertToJavaBitmap(bitmap);
    result = Java_WebappsIconUtils_generateAdaptiveIconBitmap(env, java_bitmap);
  }

  return result.obj()
             ? gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(result))
             : SkBitmap();
}

int WebappsIconUtils::GetIdealIconCornerRadiusPxForPromptUI() {
  return Java_WebappsIconUtils_getIdealIconCornerRadiusPxForPromptUI(
      base::android::AttachCurrentThread());
}

void WebappsIconUtils::SetIdealShortcutSizeForTesting(int size) {
  g_ideal_shortcut_icon_size = size;
}

void WebappsIconUtils::SetIconSizesForTesting(std::vector<int> sizes) {
  // This ordering must be kept up to date with the |GetIconSizes()| above.
  g_ideal_homescreen_icon_size = sizes[0];
  g_minimum_homescreen_icon_size = sizes[1];
  g_ideal_splash_image_size = sizes[2];
  g_minimum_splash_image_size = sizes[3];
  g_ideal_monochrome_icon_size = sizes[4];
  g_ideal_adaptive_launcher_icon_size = sizes[5];
  g_ideal_shortcut_icon_size = sizes[6];
}

}  // namespace webapps
