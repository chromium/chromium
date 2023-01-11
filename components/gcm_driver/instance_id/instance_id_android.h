// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_ANDROID_H_
#define COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_ANDROID_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/containers/id_map.h"
#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/gcm_driver/instance_id/instance_id.h"

namespace instance_id {

// InstanceID implementation for Android.
class InstanceIDAndroid : public InstanceID {
 public:
  // Tests depending on InstanceID that run without a nested Java message loop
  // must use this. Operations that would normally be asynchronous will instead
  // block the UI thread.
  class ScopedBlockOnAsyncTasksForTesting {
   public:
    ScopedBlockOnAsyncTasksForTesting();

    ScopedBlockOnAsyncTasksForTesting(
        const ScopedBlockOnAsyncTasksForTesting&) = delete;
    ScopedBlockOnAsyncTasksForTesting& operator=(
        const ScopedBlockOnAsyncTasksForTesting&) = delete;

    ~ScopedBlockOnAsyncTasksForTesting();

   private:
    bool previous_value_;
  };

  InstanceIDAndroid(const std::string& app_id, gcm::GCMDriver* gcm_driver);

  InstanceIDAndroid(const InstanceIDAndroid&) = delete;
  InstanceIDAndroid& operator=(const InstanceIDAndroid&) = delete;

  ~InstanceIDAndroid() override;

  // InstanceID implementation:
  void GetID(GetIDCallback callback) override;
  void GetCreationTime(GetCreationTimeCallback callback) override;
  void GetToken(const std::string& audience,
                const std::string& scope,
                base::TimeDelta time_to_live,
                std::set<Flags> flags,
                GetTokenCallback callback) override;
  void ValidateToken(const std::string& authorized_entity,
                     const std::string& scope,
                     const std::string& token,
                     ValidateTokenCallback callback) override;
  void DeleteTokenImpl(const std::string& audience,
                       const std::string& scope,
                       DeleteTokenCallback callback) override;
  void DeleteIDImpl(DeleteIDCallback callback) override;

  // Methods called from Java via JNI:
  void DidGetID(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jint request_id,
                const base::android::JavaParamRef<jstring>& jid);
  void DidGetCreationTime(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint request_id,
                          jlong creation_time_unix_ms);
  void DidGetToken(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jint request_id,
                   const base::android::JavaParamRef<jstring>& jtoken);
  void DidDeleteToken(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jint request_id,
                      jboolean success);
  void DidDeleteID(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jint request_id,
                   jboolean success);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  base::IDMap<std::unique_ptr<GetIDCallback>> get_id_callbacks_;
  base::IDMap<std::unique_ptr<GetCreationTimeCallback>>
      get_creation_time_callbacks_;
  base::IDMap<std::unique_ptr<GetTokenCallback>> get_token_callbacks_;
  base::IDMap<std::unique_ptr<DeleteTokenCallback>> delete_token_callbacks_;
  base::IDMap<std::unique_ptr<DeleteIDCallback>> delete_id_callbacks_;

  base::ThreadChecker thread_checker_;
};

}  // namespace instance_id

#endif  // COMPONENTS_GCM_DRIVER_INSTANCE_ID_INSTANCE_ID_ANDROID_H_
