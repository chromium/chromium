// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_NETWORK_LOADER_ANDROID_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_NETWORK_LOADER_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/data_sharing/public/data_sharing_network_loader.h"

using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace data_sharing {

// Helper class responsible for bridging the DataSharingNetworkLoader between
// C++ and Java.
class DataSharingNetworkLoaderAndroid {
 public:
  explicit DataSharingNetworkLoaderAndroid(
      DataSharingNetworkLoader* data_sharing_network_loader);
  ~DataSharingNetworkLoaderAndroid();

  void LoadUrl(JNIEnv* env,
               const JavaRef<jobject>& j_url,
               const JavaRef<jobjectArray>& j_scopes,
               const JavaRef<jbyteArray>& j_post_data,
               jint j_network_annotation_hash_code,
               const JavaRef<jobject>& j_callback);

  // Gets the java side object.
  ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  // Called when network response is received.
  void OnResponseAvailable(
      ScopedJavaGlobalRef<jobject> j_callback,
      std::unique_ptr<DataSharingNetworkLoader::LoadResult> response);

  // A reference to the Java counterpart of this class.  See
  // DataSharingNetworkLoaderImpl.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<DataSharingNetworkLoader> data_sharing_network_loader_;

  base::WeakPtrFactory<DataSharingNetworkLoaderAndroid> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_NETWORK_LOADER_ANDROID_H_
