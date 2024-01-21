// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_SERVICE_ANDROID_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_SERVICE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/data_sharing/public/data_sharing_service.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace data_sharing {

class DataSharingNetworkLoaderAndroid;

// Helper class responsible for bridging the DataSharingService between
// C++ and Java.
class DataSharingServiceAndroid : public base::SupportsUserData::Data {
 public:
  explicit DataSharingServiceAndroid(DataSharingService* service);
  ~DataSharingServiceAndroid() override;

  bool IsEmptyService(JNIEnv* env, const JavaParamRef<jobject>& j_caller);
  ScopedJavaLocalRef<jobject> GetNetworkLoader(JNIEnv* env);

  ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  // A reference to the Java counterpart of this class.  See
  // DataSharingServiceImpl.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<DataSharingService> data_sharing_service_;

  std::unique_ptr<DataSharingNetworkLoaderAndroid> network_loader_;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_SERVICE_ANDROID_H_
