// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/android/data_sharing_network_loader_android.h"

#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/data_sharing/public/android/conversion_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/data_sharing/internal/jni_headers/DataSharingNetworkLoaderImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace data_sharing {

DataSharingNetworkLoaderAndroid::DataSharingNetworkLoaderAndroid(
    DataSharingNetworkLoader* data_sharing_network_loader)
    : data_sharing_network_loader_(data_sharing_network_loader) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_DataSharingNetworkLoaderImpl_create(
                           env, reinterpret_cast<int64_t>(this))
                           .obj());
}

DataSharingNetworkLoaderAndroid::~DataSharingNetworkLoaderAndroid() {
  Java_DataSharingNetworkLoaderImpl_clearNativePtr(AttachCurrentThread(),
                                                   java_obj_);
}

void DataSharingNetworkLoaderAndroid::LoadUrl(
    JNIEnv* env,
    const JavaRef<jobject>& j_url,
    const JavaRef<jobjectArray>& j_scopes,
    const JavaRef<jbyteArray>& j_post_data,
    jint j_network_annotation_hash_code,
    const JavaRef<jobject>& j_callback) {
  if (!data_sharing_network_loader_) {
    OnResponseAvailable(ScopedJavaGlobalRef<jobject>(j_callback), nullptr);
    return;
  }
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  std::vector<std::string> scopes;
  if (j_scopes) {
    base::android::AppendJavaStringArrayToStringVector(env, j_scopes, &scopes);
  }
  std::string post_body;
  base::android::JavaByteArrayToString(env, j_post_data, &post_body);

  data_sharing_network_loader_->LoadUrl(
      url, scopes, post_body,
      net::NetworkTrafficAnnotationTag::FromJavaAnnotation(
          j_network_annotation_hash_code),
      base::BindOnce(&DataSharingNetworkLoaderAndroid::OnResponseAvailable,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_callback)));
}

ScopedJavaLocalRef<jobject> DataSharingNetworkLoaderAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

void DataSharingNetworkLoaderAndroid::OnResponseAvailable(
    ScopedJavaGlobalRef<jobject> j_callback,
    std::unique_ptr<DataSharingNetworkLoader::LoadResult> response) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> result =
      conversion::CreateDataSharingNetworkResult(env, response.get());

  base::android::RunObjectCallbackAndroid(j_callback, result);
}

}  // namespace data_sharing
