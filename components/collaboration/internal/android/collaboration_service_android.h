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
class CollaborationServiceAndroid : public base::SupportsUserData::Data {
 public:
  explicit CollaborationServiceAndroid(CollaborationService* service);
  ~CollaborationServiceAndroid() override;

  // CollaborationService Java API methods, implemented by native service:
  bool IsEmptyService(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& j_caller);
  base::android::ScopedJavaLocalRef<jobject> GetServiceStatus(JNIEnv* env);
  jint GetCurrentUserRoleForGroup(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& group_id);

  // Returns the CollaborationServiceImpl java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  // A reference to the Java counterpart of this class.  See
  // CollaborationServiceImpl.java.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned.
  raw_ptr<CollaborationService> collaboration_service_;
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_INTERNAL_ANDROID_COLLABORATION_SERVICE_ANDROID_H_
