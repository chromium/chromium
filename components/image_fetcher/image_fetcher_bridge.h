// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_BRIDGE_H_
#define COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_BRIDGE_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "ui/gfx/image/image.h"

namespace image_fetcher {

// Native counterpart of ImageFetcherBridge.java.
class ImageFetcherBridge {
 public:
  // Not copyable or movable
  ImageFetcherBridge(const ImageFetcherBridge&) = delete;
  ImageFetcherBridge& operator=(const ImageFetcherBridge&) = delete;
  ImageFetcherBridge(ImageFetcherBridge&&) = delete;
  ImageFetcherBridge& operator=(ImageFetcherBridge&&) = delete;

  static base::android::ScopedJavaLocalRef<jstring> GetFilePath(
      JNIEnv* j_env,
      const base::android::JavaParamRef<jobject>& j_simple_factory_key,
      const base::android::JavaParamRef<jstring>& j_url);

  static void FetchImageData(
      JNIEnv* j_env,
      const base::android::JavaParamRef<jobject>& j_simple_factory_key,
      const jint j_image_fetcher_config,
      const base::android::JavaParamRef<jstring>& j_url,
      const base::android::JavaParamRef<jstring>& j_client_name,
      const jint j_expiration_interval_mins,
      const base::android::JavaParamRef<jobject>& j_callback);

  static void FetchImage(
      JNIEnv* j_env,
      const base::android::JavaParamRef<jobject>& j_simple_factory_key,
      const jint j_image_fetcher_config,
      const base::android::JavaParamRef<jstring>& j_url,
      const base::android::JavaParamRef<jstring>& j_client_name,
      const jint j_frame_width,
      const jint j_frame_height,
      const jint j_expiration_interval_mins,
      const base::android::JavaParamRef<jobject>& j_callback);

  static void ReportEvent(
      JNIEnv* j_env,
      const base::android::JavaParamRef<jstring>& j_client_name,
      const jint j_event_id);

  static void ReportCacheHitTime(
      JNIEnv* j_env,
      const base::android::JavaParamRef<jstring>& j_client_name,
      const jlong start_time_millis);

  static void ReportTotalFetchTimeFromNative(
      JNIEnv* j_env,
      const base::android::JavaParamRef<jstring>& j_client_name,
      const jlong start_time_millis);

 private:
  ImageFetcherBridge();
  ~ImageFetcherBridge();

  static void OnImageDataFetched(
      base::android::ScopedJavaGlobalRef<jobject> callback,
      const std::string& image_data,
      const RequestMetadata& request_metadata);

  static void OnImageFetched(
      base::android::ScopedJavaGlobalRef<jobject> callback,
      const gfx::Image& image,
      const RequestMetadata& request_metadata);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_IMAGE_FETCHER_BRIDGE_H_
