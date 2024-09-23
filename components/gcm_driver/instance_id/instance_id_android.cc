// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/instance_id/instance_id_android.h"

#include <stdint.h>
#include <memory>
#include <numeric>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/gcm_driver/instance_id/android/jni_headers/InstanceIDBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace instance_id {

InstanceIDAndroid::ScopedBlockOnAsyncTasksForTesting::
    ScopedBlockOnAsyncTasksForTesting() {
  JNIEnv* env = AttachCurrentThread();
  previous_value_ =
      Java_InstanceIDBridge_setBlockOnAsyncTasksForTesting(env, true);
}

InstanceIDAndroid::ScopedBlockOnAsyncTasksForTesting::
    ~ScopedBlockOnAsyncTasksForTesting() {
  JNIEnv* env = AttachCurrentThread();
  Java_InstanceIDBridge_setBlockOnAsyncTasksForTesting(env, previous_value_);
}

// static
std::unique_ptr<InstanceID> InstanceID::CreateInternal(
    const std::string& app_id,
    gcm::GCMDriver* gcm_driver) {
  return std::make_unique<InstanceIDAndroid>(app_id, gcm_driver);
}

InstanceIDAndroid::InstanceIDAndroid(const std::string& app_id,
                                     gcm::GCMDriver* gcm_driver)
    : InstanceID(app_id, gcm_driver) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(!app_id.empty()) << "Empty app_id is not supported";
  // The |app_id| is stored in GCM's category field by the desktop InstanceID
  // implementation, but because the category is reserved for the app's package
  // name on Android the subtype field is used instead.
  std::string subtype = app_id;

  JNIEnv* env = AttachCurrentThread();
  java_ref_.Reset(
      Java_InstanceIDBridge_create(env, reinterpret_cast<intptr_t>(this),
                                   ConvertUTF8ToJavaString(env, subtype)));
}

InstanceIDAndroid::~InstanceIDAndroid() {
  DCHECK(thread_checker_.CalledOnValidThread());

  JNIEnv* env = AttachCurrentThread();
  Java_InstanceIDBridge_destroy(env, java_ref_);
}

void InstanceIDAndroid::GetID(GetIDCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  int32_t request_id = get_id_callbacks_.Add(
      std::make_unique<GetIDCallback>(std::move(callback)));

  JNIEnv* env = AttachCurrentThread();
  Java_InstanceIDBridge_getId(env, java_ref_, request_id);
}

void InstanceIDAndroid::GetCreationTime(GetCreationTimeCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  int32_t request_id = get_creation_time_callbacks_.Add(
      std::make_unique<GetCreationTimeCallback>(std::move(callback)));

  JNIEnv* env = AttachCurrentThread();
  Java_InstanceIDBridge_getCreationTime(env, java_ref_, request_id);
}

void InstanceIDAndroid::GetToken(
    const std::string& authorized_entity,
    const std::string& scope,
    base::TimeDelta time_to_live,
    std::set<Flags> flags,
    GetTokenCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!time_to_live.is_zero()) {
    LOG(WARNING) << "Non-zero TTL requested for InstanceID token, while TTLs"
                    " are not supported by Android Firebase IID API.";
  }

  int32_t request_id = get_token_callbacks_.Add(
      std::make_unique<GetTokenCallback>(std::move(callback)));

  int java_flags = std::accumulate(
      flags.begin(), flags.end(), 0,
      [](int sum, Flags flag) { return sum + static_cast<int>(flag); });

  JNIEnv* env = AttachCurrentThread();
  Java_InstanceIDBridge_getToken(
      env, java_ref_, request_id,
      ConvertUTF8ToJavaString(env, authorized_entity),
      ConvertUTF8ToJavaString(env, scope), java_flags);
}

void InstanceIDAndroid::ValidateToken(const std::string& authorized_entity,
                                      const std::string& scope,
                                      const std::string& token,
                                      ValidateTokenCallback callback) {
  // gcm_driver doesn't store tokens on Android, so assume it's valid.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /* is_valid */));
}

void InstanceIDAndroid::DeleteTokenImpl(const std::string& authorized_entity,
                                        const std::string& scope,
                                        DeleteTokenCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  int32_t request_id = delete_token_callbacks_.Add(
      std::make_unique<DeleteTokenCallback>(std::move(callback)));

  JNIEnv* env = AttachCurrentThread();
  Java_InstanceIDBridge_deleteToken(
      env, java_ref_, request_id,
      ConvertUTF8ToJavaString(env, authorized_entity),
      ConvertUTF8ToJavaString(env, scope));
}

void InstanceIDAndroid::DeleteIDImpl(DeleteIDCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  int32_t request_id = delete_id_callbacks_.Add(
      std::make_unique<DeleteIDCallback>(std::move(callback)));

  JNIEnv* env = AttachCurrentThread();
  Java_InstanceIDBridge_deleteInstanceID(env, java_ref_, request_id);
}

void InstanceIDAndroid::DidGetID(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint request_id,
    const base::android::JavaParamRef<jstring>& jid) {
  DCHECK(thread_checker_.CalledOnValidThread());

  GetIDCallback* callback = get_id_callbacks_.Lookup(request_id);
  DCHECK(callback);
  std::move(*callback).Run(ConvertJavaStringToUTF8(jid));
  get_id_callbacks_.Remove(request_id);
}

void InstanceIDAndroid::DidGetCreationTime(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint request_id,
    jlong creation_time_unix_ms) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::Time creation_time;
  // If the InstanceID's getId, getToken and deleteToken methods have never been
  // called, or deleteInstanceID has cleared it since, creation time will be 0.
  if (creation_time_unix_ms) {
    creation_time =
        base::Time::UnixEpoch() + base::Milliseconds(creation_time_unix_ms);
  }

  GetCreationTimeCallback* callback =
      get_creation_time_callbacks_.Lookup(request_id);
  DCHECK(callback);
  std::move(*callback).Run(creation_time);
  get_creation_time_callbacks_.Remove(request_id);
}

void InstanceIDAndroid::DidGetToken(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint request_id,
    const base::android::JavaParamRef<jstring>& jtoken) {
  DCHECK(thread_checker_.CalledOnValidThread());

  GetTokenCallback* callback = get_token_callbacks_.Lookup(request_id);
  DCHECK(callback);
  std::string token = ConvertJavaStringToUTF8(jtoken);
  std::move(*callback).Run(
      token, token.empty() ? InstanceID::UNKNOWN_ERROR : InstanceID::SUCCESS);
  get_token_callbacks_.Remove(request_id);
}

void InstanceIDAndroid::DidDeleteToken(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint request_id,
    jboolean success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DeleteTokenCallback* callback = delete_token_callbacks_.Lookup(request_id);
  DCHECK(callback);
  std::move(*callback).Run(success ? InstanceID::SUCCESS
                                   : InstanceID::UNKNOWN_ERROR);
  delete_token_callbacks_.Remove(request_id);
}

void InstanceIDAndroid::DidDeleteID(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint request_id,
    jboolean success) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DeleteIDCallback* callback = delete_id_callbacks_.Lookup(request_id);
  DCHECK(callback);
  std::move(*callback).Run(success ? InstanceID::SUCCESS
                                   : InstanceID::UNKNOWN_ERROR);
  delete_id_callbacks_.Remove(request_id);
}

}  // namespace instance_id
