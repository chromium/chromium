// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H_
#define COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_stats_recorder_android.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace gcm {

// GCMDriver implementation for Android, using Android GCM APIs.
class GCMDriverAndroid : public GCMDriver,
                         public GCMStatsRecorderAndroid::Delegate {
 public:
  GCMDriverAndroid(
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

  GCMDriverAndroid(const GCMDriverAndroid&) = delete;
  GCMDriverAndroid& operator=(const GCMDriverAndroid&) = delete;

  ~GCMDriverAndroid() override;

  // Methods called from Java via JNI:
  void OnRegisterFinished(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& app_id,
      const base::android::JavaParamRef<jstring>& registration_id,
      jboolean success);
  void OnUnregisterFinished(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            const base::android::JavaParamRef<jstring>& app_id,
                            jboolean success);
  void OnMessageReceived(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& app_id,
      const base::android::JavaParamRef<jstring>& sender_id,
      const base::android::JavaParamRef<jstring>& j_message_id,
      const base::android::JavaParamRef<jstring>& collapse_key,
      const base::android::JavaParamRef<jbyteArray>& raw_data,
      const base::android::JavaParamRef<jobjectArray>& data_keys_and_values);

  // GCMDriver implementation:
  void ValidateRegistration(const std::string& app_id,
                            const std::vector<std::string>& sender_ids,
                            const std::string& registration_id,
                            ValidateRegistrationCallback callback) override;
  void AddConnectionObserver(GCMConnectionObserver* observer) override;
  void RemoveConnectionObserver(GCMConnectionObserver* observer) override;
  GCMClient* GetGCMClientForTesting() const override;
  bool IsStarted() const override;
  bool IsConnected() const override;
  void GetGCMStatistics(GetGCMStatisticsCallback callback,
                        ClearActivityLogs clear_logs) override;
  void SetGCMRecording(const GCMStatisticsRecordingCallback& callback,
                       bool recording) override;
  void SetAccountTokens(
      const std::vector<GCMClient::AccountTokenInfo>& account_tokens) override;
  void UpdateAccountMapping(const AccountMapping& account_mapping) override;
  void RemoveAccountMapping(const CoreAccountId& account_id) override;
  base::Time GetLastTokenFetchTime() override;
  void SetLastTokenFetchTime(const base::Time& time) override;
  InstanceIDHandler* GetInstanceIDHandlerInternal() override;
  void AddHeartbeatInterval(const std::string& scope, int interval_ms) override;
  void RemoveHeartbeatInterval(const std::string& scope) override;
  void AddAppHandler(const std::string& app_id,
                     GCMAppHandler* handler) override;

  // GCMStatsRecorder::Delegate implementation:
  void OnActivityRecorded() override;

 protected:
  // GCMDriver implementation:
  GCMClient::Result EnsureStarted(GCMClient::StartMode start_mode) override;
  void RegisterImpl(const std::string& app_id,
                    const std::vector<std::string>& sender_ids) override;
  void UnregisterImpl(const std::string& app_id) override;
  void UnregisterWithSenderIdImpl(const std::string& app_id,
                                  const std::string& sender_id) override;
  void SendImpl(const std::string& app_id,
                const std::string& receiver_id,
                const OutgoingMessage& message) override;
  void RecordDecryptionFailure(const std::string& app_id,
                               GCMDecryptionResult result) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Callback for SetGCMRecording.
  GCMStatisticsRecordingCallback gcm_statistics_recording_callback_;

  // Recorder that logs GCM activities.
  GCMStatsRecorderAndroid recorder_;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_DRIVER_ANDROID_H_
