// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_ANDROID_COLLABORATION_SERVICE_ANDROID_H_
#define COMPONENTS_COLLABORATION_INTERNAL_ANDROID_COLLABORATION_SERVICE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/collaboration/public/collaboration_service.h"

namespace collaboration {

// Helper class responsible for bridging the CollaborationService between
// C++ and Java.
class CollaborationServiceAndroid : public base::SupportsUserData::Data,
                                    public CollaborationService::Observer {
 public:
  explicit CollaborationServiceAndroid(CollaborationService* service);
  ~CollaborationServiceAndroid() override;

  // CollaborationService Java API methods, implemented by native service:
  bool IsEmptyService(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& j_caller);
  void StartJoinFlow(JNIEnv* env,
                     jlong delegate,
                     const base::android::JavaParamRef<jobject>& j_url);
  void StartShareOrManageFlow(
      JNIEnv* env,
      jlong delegate,
      const base::android::JavaParamRef<jstring>& j_sync_group_id,
      const base::android::JavaParamRef<jobject>& j_local_group_id,
      jint entry);
  void StartLeaveOrDeleteFlow(
      JNIEnv* env,
      jlong delegate,
      const base::android::JavaParamRef<jstring>& j_sync_group_id,
      const base::android::JavaParamRef<jobject>& j_local_group_id,
      jint entry);
  base::android::ScopedJavaLocalRef<jobject> GetServiceStatus(JNIEnv* env);
  jint GetCurrentUserRoleForGroup(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& group_id);
  jni_zero::ScopedJavaLocalRef<jobject> GetGroupData(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& group_id);
  void LeaveGroup(JNIEnv* env,
                  const base::android::JavaParamRef<jstring>& group_id,
                  const base::android::JavaParamRef<jobject>& j_callback);
  void DeleteGroup(JNIEnv* env,
                   const base::android::JavaParamRef<jstring>& group_id,
                   const base::android::JavaParamRef<jobject>& j_callback);

  // Returns the CollaborationServiceImpl java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // CollaborationService::Observer overrides.
  void OnServiceStatusChanged(const ServiceStatusUpdate& update) override;

 private:
  // A reference to the Java counterpart of this class.  See
  // CollaborationServiceImpl.java.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<CollaborationService> collaboration_service_;
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_INTERNAL_ANDROID_COLLABORATION_SERVICE_ANDROID_H_
