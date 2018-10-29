// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_android.h"

#include <stddef.h>
#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jni/GCMDriver_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaByteArrayToString;
using base::android::JavaParamRef;

namespace gcm {

 GCMDriverAndroid::GCMDriverAndroid(
     const base::FilePath& store_path,
     const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
     : GCMDriver(store_path, blocking_task_runner),
       recorder_(this) {
  JNIEnv* env = AttachCurrentThread();
  java_ref_.Reset(Java_GCMDriver_create(env, reinterpret_cast<intptr_t>(this)));
}

GCMDriverAndroid::~GCMDriverAndroid() {
  JNIEnv* env = AttachCurrentThread();
  Java_GCMDriver_destroy(env, java_ref_);
}

void GCMDriverAndroid::OnRegisterFinished(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_app_id,
    const JavaParamRef<jstring>& j_registration_id,
    jboolean success) {
  std::string app_id = ConvertJavaStringToUTF8(env, j_app_id);
  std::string registration_id = ConvertJavaStringToUTF8(env, j_registration_id);
  GCMClient::Result result = success ? GCMClient::SUCCESS
                                     : GCMClient::UNKNOWN_ERROR;

  recorder_.RecordRegistrationResponse(app_id, success);

  RegisterFinished(app_id, registration_id, result);
}

void GCMDriverAndroid::OnUnregisterFinished(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_app_id,
    jboolean success) {
  std::string app_id = ConvertJavaStringToUTF8(env, j_app_id);
  GCMClient::Result result = success ? GCMClient::SUCCESS
                                     : GCMClient::UNKNOWN_ERROR;

  recorder_.RecordUnregistrationResponse(app_id, success);

  RemoveEncryptionInfoAfterUnregister(app_id, result);
}

void GCMDriverAndroid::OnMessageReceived(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_app_id,
    const JavaParamRef<jstring>& j_sender_id,
    const JavaParamRef<jstring>& j_collapse_key,
    const JavaParamRef<jbyteArray>& j_raw_data,
    const JavaParamRef<jobjectArray>& j_data_keys_and_values) {
  std::string app_id = ConvertJavaStringToUTF8(env, j_app_id);

  int message_byte_size = 0;

  IncomingMessage message;
  message.sender_id = ConvertJavaStringToUTF8(env, j_sender_id);
  if (!j_collapse_key.is_null())
    ConvertJavaStringToUTF8(env, j_collapse_key, &message.collapse_key);

  // Expand j_data_keys_and_values from array to map.
  std::vector<std::string> data_keys_and_values;
  AppendJavaStringArrayToStringVector(env,
                                      j_data_keys_and_values,
                                      &data_keys_and_values);
  for (size_t i = 0; i + 1 < data_keys_and_values.size(); i += 2) {
    message.data[data_keys_and_values[i]] = data_keys_and_values[i+1];
    message_byte_size += data_keys_and_values[i+1].size();
  }
  // Convert j_raw_data from byte[] to binary std::string.
  if (j_raw_data) {
    JavaByteArrayToString(env, j_raw_data, &message.raw_data);

    message_byte_size += message.raw_data.size();
  }

  recorder_.RecordDataMessageReceived(app_id, message.sender_id,
                                      message_byte_size);

  DispatchMessage(app_id, message);
}

void GCMDriverAndroid::ValidateRegistration(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const std::string& registration_id,
    const ValidateRegistrationCallback& callback) {
  // gcm_driver doesn't store registration IDs on Android, so assume it's valid.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, true /* is_valid */));
}

void GCMDriverAndroid::OnSignedIn() {
}

void GCMDriverAndroid::OnSignedOut() {
}

void GCMDriverAndroid::AddConnectionObserver(GCMConnectionObserver* observer) {
}

void GCMDriverAndroid::RemoveConnectionObserver(
    GCMConnectionObserver* observer) {
}

void GCMDriverAndroid::Enable() {
}

void GCMDriverAndroid::Disable() {
}

GCMClient* GCMDriverAndroid::GetGCMClientForTesting() const {
  NOTIMPLEMENTED();
  return NULL;
}

bool GCMDriverAndroid::IsStarted() const {
  return true;
}

bool GCMDriverAndroid::IsConnected() const {
  // TODO(gcm): hook up to GCM connected status
  return true;
}

void GCMDriverAndroid::GetGCMStatistics(
    const GetGCMStatisticsCallback& callback,
    ClearActivityLogs clear_logs) {
  DCHECK(!callback.is_null());

  get_gcm_statistics_callback_ = callback;

  if (clear_logs == CLEAR_LOGS)
    recorder_.Clear();

  GCMClient::GCMStatistics stats;
  stats.is_recording = recorder_.is_recording();

  recorder_.CollectActivities(&stats.recorded_activities);

  callback.Run(stats);
}

void GCMDriverAndroid::SetGCMRecording(const GetGCMStatisticsCallback& callback,
                                       bool recording) {
  DCHECK(!callback.is_null());

  recorder_.set_is_recording(recording);

  GetGCMStatistics(callback, recording ? KEEP_LOGS : CLEAR_LOGS);
}

void GCMDriverAndroid::SetAccountTokens(
    const std::vector<GCMClient::AccountTokenInfo>& account_tokens) {
  NOTIMPLEMENTED();
}

void GCMDriverAndroid::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
  NOTIMPLEMENTED();
}

void GCMDriverAndroid::RemoveAccountMapping(const std::string& account_id) {
  NOTIMPLEMENTED();
}

base::Time GCMDriverAndroid::GetLastTokenFetchTime() {
  NOTIMPLEMENTED();
  return base::Time();
}

void GCMDriverAndroid::SetLastTokenFetchTime(const base::Time& time) {
  NOTIMPLEMENTED();
}

void GCMDriverAndroid::WakeFromSuspendForHeartbeat(bool wake) {
}

InstanceIDHandler* GCMDriverAndroid::GetInstanceIDHandlerInternal() {
  // Not supported for Android.
  return NULL;
}

void GCMDriverAndroid::AddHeartbeatInterval(const std::string& scope,
                                            int interval_ms) {
}

void GCMDriverAndroid::RemoveHeartbeatInterval(const std::string& scope) {
}

void GCMDriverAndroid::OnActivityRecorded() {
  DCHECK(!get_gcm_statistics_callback_.is_null());

  GetGCMStatistics(get_gcm_statistics_callback_, KEEP_LOGS);
}

GCMClient::Result GCMDriverAndroid::EnsureStarted(
    GCMClient::StartMode start_mode) {
  // TODO(johnme): Maybe we should check if GMS is available?
  return GCMClient::SUCCESS;
}

void GCMDriverAndroid::RegisterImpl(
    const std::string& app_id, const std::vector<std::string>& sender_ids) {
  DCHECK_EQ(1u, sender_ids.size());
  JNIEnv* env = AttachCurrentThread();

  recorder_.RecordRegistrationSent(app_id);

  Java_GCMDriver_register(env, java_ref_, ConvertUTF8ToJavaString(env, app_id),
                          ConvertUTF8ToJavaString(env, sender_ids[0]));
}

void GCMDriverAndroid::UnregisterImpl(const std::string& app_id) {
  NOTREACHED();
}

void GCMDriverAndroid::UnregisterWithSenderIdImpl(
    const std::string& app_id,
    const std::string& sender_id) {
  JNIEnv* env = AttachCurrentThread();

  recorder_.RecordUnregistrationSent(app_id);

  Java_GCMDriver_unregister(env, java_ref_,
                            ConvertUTF8ToJavaString(env, app_id),
                            ConvertUTF8ToJavaString(env, sender_id));
}

void GCMDriverAndroid::SendImpl(const std::string& app_id,
                                const std::string& receiver_id,
                                const OutgoingMessage& message) {
  NOTIMPLEMENTED();
}

void GCMDriverAndroid::RecordDecryptionFailure(const std::string& app_id,
                                               GCMDecryptionResult result) {
  recorder_.RecordDecryptionFailure(app_id, result);
}

}  // namespace gcm
