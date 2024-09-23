// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/image_fetcher_bridge.h"

#include <jni.h>

#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/embedder_support/android/simple_factory_key/simple_factory_key_handle.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/features.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_metrics_reporter.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/image_fetcher_service_provider.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/image_fetcher/jni_headers/ImageFetcherBridge_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace image_fetcher {

namespace {

// Keep in sync with postfix found in image_data_store_disk.cc.
const base::FilePath::CharType kPathPostfix[] =
    FILE_PATH_LITERAL("image_data_storage");

// TODO(wylieb): Allow java clients to map to a traffic_annotation here.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cached_image_fetcher", R"(
        semantics {
          sender: "Cached Image Fetcher Fetch"
          description:
            "Fetches and caches images for Chrome features."
          trigger:
            "Triggered when a feature requests an image fetch."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Cache can be cleared through settings."
        policy_exception_justification:
          "This feature allows many different Chrome features to fetch/cache "
          "images and thus there is no Chrome-wide policy to disable it."
      })");

data_decoder::DataDecoder* GetDataDecoder() {
  static base::NoDestructor<data_decoder::DataDecoder> data_decoder;
  return data_decoder.get();
}

}  // namespace

ImageFetcherBridge::~ImageFetcherBridge() = default;

// static
ScopedJavaLocalRef<jstring> ImageFetcherBridge::GetFilePath(
    JNIEnv* j_env,
    const JavaParamRef<jobject>& j_simple_factory_key,
    const JavaParamRef<jstring>& j_url) {
  std::string url = base::android::ConvertJavaStringToUTF8(j_url);
  SimpleFactoryKey* key =
      simple_factory_key::SimpleFactoryKeyFromJavaHandle(j_simple_factory_key);
  std::string file_path = image_fetcher::GetImageFetcherCachePath(
      key, base::FilePath(kPathPostfix)
               .Append(ImageCache::HashUrlToKey(url))
               .MaybeAsASCII());
  return base::android::ConvertUTF8ToJavaString(j_env, file_path);
}

// static
void ImageFetcherBridge::FetchImageData(
    JNIEnv* j_env,
    const JavaParamRef<jobject>& j_simple_factory_key,
    const jint j_image_fetcher_config,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_client_name,
    const jint j_expiration_interval_mins,
    const JavaParamRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  ImageFetcherConfig config =
      static_cast<ImageFetcherConfig>(j_image_fetcher_config);
  std::string url = base::android::ConvertJavaStringToUTF8(j_url);
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);

  image_fetcher::ImageFetcherParams params(kTrafficAnnotation, client_name);
  if (j_expiration_interval_mins > 0) {
    params.set_hold_for_expiration_interval(
        base::Minutes(j_expiration_interval_mins));
  }
  if (base::FeatureList::IsEnabled(features::kBatchImageDecoding))
    params.set_data_decoder(GetDataDecoder());

  // We can skip transcoding here because this method is used in java as
  // ImageFetcher.fetchGif, which decodes the data in a Java-only library.
  params.set_skip_transcoding(true);
  SimpleFactoryKey* key =
      simple_factory_key::SimpleFactoryKeyFromJavaHandle(j_simple_factory_key);
  image_fetcher::GetImageFetcherService(key)
      ->GetImageFetcher(config)
      ->FetchImageData(
          GURL(url),
          base::BindOnce(&ImageFetcherBridge::OnImageDataFetched, callback),
          std::move(params));
}

// static
void ImageFetcherBridge::FetchImage(
    JNIEnv* j_env,
    const JavaParamRef<jobject>& j_simple_factory_key,
    const jint j_image_fetcher_config,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_client_name,
    const jint j_frame_width,
    const jint j_frame_height,
    const jint j_expiration_interval_mins,
    const JavaParamRef<jobject>& j_callback) {
  ScopedJavaGlobalRef<jobject> callback(j_callback);
  ImageFetcherConfig config =
      static_cast<ImageFetcherConfig>(j_image_fetcher_config);
  std::string url = base::android::ConvertJavaStringToUTF8(j_url);
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);

  ImageFetcherParams params(kTrafficAnnotation, client_name);
  params.set_frame_size(gfx::Size(j_frame_width, j_frame_height));
  if (j_expiration_interval_mins > 0) {
    params.set_hold_for_expiration_interval(
        base::Minutes(j_expiration_interval_mins));
  }
  if (base::FeatureList::IsEnabled(features::kBatchImageDecoding))
    params.set_data_decoder(GetDataDecoder());

  SimpleFactoryKey* key =
      simple_factory_key::SimpleFactoryKeyFromJavaHandle(j_simple_factory_key);
  image_fetcher::GetImageFetcherService(key)
      ->GetImageFetcher(config)
      ->FetchImage(
          GURL(url),
          base::BindOnce(&ImageFetcherBridge::OnImageFetched, callback),
          std::move(params));
}

// static
void ImageFetcherBridge::ReportEvent(
    JNIEnv* j_env,
    const base::android::JavaParamRef<jstring>& j_client_name,
    const jint j_event_id) {
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);
  ImageFetcherEvent event = static_cast<ImageFetcherEvent>(j_event_id);
  ImageFetcherMetricsReporter::ReportEvent(client_name, event);
}

// static
void ImageFetcherBridge::ReportCacheHitTime(
    JNIEnv* j_env,
    const base::android::JavaParamRef<jstring>& j_client_name,
    const jlong start_time_millis) {
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);
  base::Time start_time =
      base::Time::FromMillisecondsSinceUnixEpoch(start_time_millis);
  ImageFetcherMetricsReporter::ReportImageLoadFromCacheTimeJava(client_name,
                                                                start_time);
}

// static
void ImageFetcherBridge::ReportTotalFetchTimeFromNative(
    JNIEnv* j_env,
    const base::android::JavaParamRef<jstring>& j_client_name,
    const jlong start_time_millis) {
  std::string client_name =
      base::android::ConvertJavaStringToUTF8(j_client_name);
  base::Time start_time =
      base::Time::FromMillisecondsSinceUnixEpoch(start_time_millis);
  ImageFetcherMetricsReporter::ReportTotalFetchFromNativeTimeJava(client_name,
                                                                  start_time);
}

// ------------------ JNI functions ------------------
// static
ScopedJavaLocalRef<jstring> JNI_ImageFetcherBridge_GetFilePath(
    JNIEnv* j_env,
    const JavaParamRef<jobject>& j_simple_factory_key,
    const JavaParamRef<jstring>& j_url) {
  return ImageFetcherBridge::GetFilePath(j_env, j_simple_factory_key, j_url);
}

// static
void JNI_ImageFetcherBridge_FetchImageData(
    JNIEnv* j_env,
    const JavaParamRef<jobject>& j_simple_factory_key,
    const jint j_image_fetcher_config,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_client_name,
    const jint j_expiration_interval_mins,
    const JavaParamRef<jobject>& j_callback) {
  ImageFetcherBridge::FetchImageData(
      j_env, j_simple_factory_key, j_image_fetcher_config, j_url, j_client_name,
      j_expiration_interval_mins, j_callback);
}

// static
void JNI_ImageFetcherBridge_FetchImage(
    JNIEnv* j_env,
    const JavaParamRef<jobject>& j_simple_factory_key,
    const jint j_image_fetcher_config,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_client_name,
    const jint j_frame_width,
    const jint j_frame_height,
    const jint j_expiration_interval_mins,
    const JavaParamRef<jobject>& j_callback) {
  ImageFetcherBridge::FetchImage(
      j_env, j_simple_factory_key, j_image_fetcher_config, j_url, j_client_name,
      j_frame_width, j_frame_height, j_expiration_interval_mins, j_callback);
}

// static
void JNI_ImageFetcherBridge_ReportEvent(
    JNIEnv* j_env,
    const base::android::JavaParamRef<jstring>& j_client_name,
    const jint j_event_id) {
  ImageFetcherBridge::ReportEvent(j_env, j_client_name, j_event_id);
}

// static
void JNI_ImageFetcherBridge_ReportCacheHitTime(
    JNIEnv* j_env,
    const base::android::JavaParamRef<jstring>& j_client_name,
    const jlong start_time_millis) {
  ImageFetcherBridge::ReportCacheHitTime(j_env, j_client_name,
                                         start_time_millis);
}

// static
void JNI_ImageFetcherBridge_ReportTotalFetchTimeFromNative(
    JNIEnv* j_env,
    const base::android::JavaParamRef<jstring>& j_client_name,
    const jlong start_time_millis) {
  ImageFetcherBridge::ReportTotalFetchTimeFromNative(j_env, j_client_name,
                                                     start_time_millis);
}

// ------------------ Private functions ------------------

// static
void ImageFetcherBridge::OnImageDataFetched(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const std::string& image_data,
    const RequestMetadata& request_metadata) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_bytes =
      base::android::ToJavaByteArray(env, image_data);

  base::android::RunObjectCallbackAndroid(callback, j_bytes);
}

// static
void ImageFetcherBridge::OnImageFetched(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const gfx::Image& image,
    const RequestMetadata& request_metadata) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty()) {
    j_bitmap = gfx::ConvertToJavaBitmap(*image.ToSkBitmap());
  }
  base::android::RunObjectCallbackAndroid(callback, j_bitmap);
}

}  // namespace image_fetcher
