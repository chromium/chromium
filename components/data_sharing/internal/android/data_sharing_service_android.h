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

using base::android::JavaRef;
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

  // DataSharingService Java API methods, implemented by native service:
  void ReadGroup(JNIEnv* env,
                 const JavaRef<jstring>& group_id,
                 const JavaRef<jobject>& j_callback);
  void CreateGroup(JNIEnv* env,
                   const JavaRef<jstring>& group_name,
                   const JavaRef<jobject>& j_callback);
  void InviteMember(JNIEnv* env,
                    const JavaRef<jstring>& group_id,
                    const JavaRef<jstring>& invitee_email,
                    const JavaRef<jobject>& j_callback);
  void AddMember(JNIEnv* env,
                 const JavaRef<jstring>& group_id,
                 const JavaRef<jstring>& access_token,
                 const JavaRef<jobject>& j_callback);
  void RemoveMember(JNIEnv* env,
                    const JavaRef<jstring>& group_id,
                    const JavaRef<jstring>& member_email,
                    const JavaRef<jobject>& j_callback);
  bool IsEmptyService(JNIEnv* env);
  ScopedJavaLocalRef<jobject> GetNetworkLoader(JNIEnv* env);
  ScopedJavaLocalRef<jobject> GetDataSharingUrl(
      JNIEnv* env,
      const JavaRef<jstring>& group_id,
      const JavaRef<jstring>& access_token);
  ScopedJavaLocalRef<jobject> ParseDataSharingUrl(
      JNIEnv* env,
      const JavaRef<jobject>& j_url);
  void EnsureGroupVisibility(JNIEnv* env,
                             const JavaRef<jstring>& group_id,
                             const JavaRef<jobject>& j_callback);
  void GetSharedEntitiesPreview(JNIEnv* env,
                                const JavaRef<jstring>& group_id,
                                const JavaRef<jstring>& access_token,
                                const JavaRef<jobject>& j_callback);
  ScopedJavaLocalRef<jobject> GetUiDelegate(JNIEnv* env);
  void Log(JNIEnv* env,
           /*logger_common::mojom::LogSource*/ int32_t source,
           const JavaRef<jstring>& message);

  void SetSharedEntitiesPreviewForTesting(JNIEnv* env,
                                          const JavaRef<jstring>& j_group_id);

  // Returns the DataSharingServiceImpl java object.
  ScopedJavaLocalRef<jobject> GetJavaObject();

  // Returns the observer that routes the notifications to Java observers. The
  // returned object is of type:
  // org.chromium.components.data_sharing.ObserverBridge.
  ScopedJavaLocalRef<jobject> GetJavaObserverBridge();

 private:
  class GroupDataObserverBridge;

  // A reference to the Java counterpart of this class.  See
  // DataSharingServiceImpl.java.
  ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<DataSharingService> data_sharing_service_;

  std::unique_ptr<DataSharingNetworkLoaderAndroid> network_loader_;

  std::unique_ptr<GroupDataObserverBridge> observer_bridge_;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_ANDROID_DATA_SHARING_SERVICE_ANDROID_H_
