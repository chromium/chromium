// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/android/data_sharing_service_android.h"

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "components/data_sharing/internal/android/data_sharing_network_loader_android.h"
#include "components/data_sharing/internal/jni_headers/DataSharingServiceImpl_jni.h"
#include "components/data_sharing/public/data_sharing_service.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace data_sharing {
namespace {

const char kDataSharingServiceBridgeKey[] = "data_sharing_service_bridge";

}  // namespace

// This function is declared in data_sharing_service.h and
// should be linked in to any binary using
// DataSharingService::GetJavaObject.
// static
ScopedJavaLocalRef<jobject> DataSharingService::GetJavaObject(
    DataSharingService* service) {
  if (!service->GetUserData(kDataSharingServiceBridgeKey)) {
    service->SetUserData(kDataSharingServiceBridgeKey,
                         std::make_unique<DataSharingServiceAndroid>(service));
  }

  DataSharingServiceAndroid* bridge = static_cast<DataSharingServiceAndroid*>(
      service->GetUserData(kDataSharingServiceBridgeKey));

  return bridge->GetJavaObject();
}

DataSharingServiceAndroid::DataSharingServiceAndroid(
    DataSharingService* data_sharing_service)
    : data_sharing_service_(data_sharing_service),
      network_loader_(std::make_unique<DataSharingNetworkLoaderAndroid>(
          data_sharing_service->GetDataSharingNetworkLoader())) {
  DCHECK(data_sharing_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_DataSharingServiceImpl_create(
                           env, reinterpret_cast<int64_t>(this))
                           .obj());
}

DataSharingServiceAndroid::~DataSharingServiceAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DataSharingServiceImpl_clearNativePtr(env, java_obj_);
}

bool DataSharingServiceAndroid::IsEmptyService(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  return data_sharing_service_->IsEmptyService();
}

ScopedJavaLocalRef<jobject> DataSharingServiceAndroid::GetNetworkLoader(
    JNIEnv* env) {
  return network_loader_->GetJavaObject();
}

ScopedJavaLocalRef<jobject> DataSharingServiceAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace data_sharing
