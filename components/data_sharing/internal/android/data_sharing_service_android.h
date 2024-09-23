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

  // DataSharingService Java API methods, implemented by native service:
  void ReadAllGroups(JNIEnv* env, const JavaParamRef<jobject>& j_callback);
  void ReadGroup(JNIEnv* env,
                 const JavaParamRef<jstring>& group_id,
                 const JavaParamRef<jobject>& j_callback);
  void CreateGroup(JNIEnv* env,
                   const JavaParamRef<jstring>& group_name,
                   const JavaParamRef<jobject>& j_callback);
  void DeleteGroup(JNIEnv* env,
                   const JavaParamRef<jstring>& group_id,
                   const JavaParamRef<jobject>& j_callback);
  void InviteMember(JNIEnv* env,
                    const JavaParamRef<jstring>& group_id,
                    const JavaParamRef<jstring>& invitee_email,
                    const JavaParamRef<jobject>& j_callback);
  void AddMember(JNIEnv* env,
                 const JavaParamRef<jstring>& group_id,
                 const JavaParamRef<jstring>& access_token,
                 const JavaParamRef<jobject>& j_callback);
  void RemoveMember(JNIEnv* env,
                    const JavaParamRef<jstring>& group_id,
                    const JavaParamRef<jstring>& member_email,
                    const JavaParamRef<jobject>& j_callback);
  bool IsEmptyService(JNIEnv* env, const JavaParamRef<jobject>& j_caller);
  ScopedJavaLocalRef<jobject> GetNetworkLoader(JNIEnv* env);
  ScopedJavaLocalRef<jobject> GetDataSharingURL(
      JNIEnv* env,
      const JavaParamRef<jstring>& group_id,
      const JavaParamRef<jstring>& access_token);
  ScopedJavaLocalRef<jobject> ParseDataSharingURL(
      JNIEnv* env,
      const JavaParamRef<jobject>& j_url);
  void EnsureGroupVisibility(JNIEnv* env,
                             const JavaParamRef<jstring>& group_id,
                             const JavaParamRef<jobject>& j_callback);
  void GetSharedEntitiesPreview(JNIEnv* env,
                                const JavaParamRef<jstring>& group_id,
                                const JavaParamRef<jstring>& access_token,
                                const JavaParamRef<jobject>& j_callback);
  ScopedJavaLocalRef<jobject> GetUIDelegate(JNIEnv* env);
  ScopedJavaLocalRef<jobject> GetServiceStatus(JNIEnv* env);

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
