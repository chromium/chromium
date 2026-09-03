// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/fcm_driver_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/fcm/fcm_message.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/fcm/android/jni_headers/FcmBridge_jni.h"

namespace fcm {
FcmDriverAndroid::FcmDriverAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(Java_FcmBridge_create(env, reinterpret_cast<intptr_t>(this)));
  Java_FcmBridge_fetchInstallationId(env, java_ref_);
}

FcmDriverAndroid::~FcmDriverAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FcmBridge_destroy(env, java_ref_);
}

void FcmDriverAndroid::OnInstallationIdRefreshed(
    JNIEnv* env,
    const std::string& installation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<InstallationId> id_opt;
  if (!installation_id.empty()) {
    id_opt = InstallationId(installation_id);
  }
  DispatchInstallationIdRefreshed(id_opt);
}

void FcmDriverAndroid::OnMessageReceived(
    JNIEnv* env,
    const std::string& message_id,
    const std::vector<std::string>& data_keys_and_values,
    const std::vector<uint8_t>& raw_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FcmMessage message;
  message.message_id = message_id;

  for (size_t i = 0; i + 1 < data_keys_and_values.size(); i += 2) {
    message.data[data_keys_and_values[i]] = data_keys_and_values[i + 1];
  }

  if (!raw_data.empty()) {
    message.raw_data.assign(raw_data.begin(), raw_data.end());
  }

  DispatchIncomingMessage(message);
}

void FcmDriverAndroid::OnMessagesDeleted(JNIEnv* env) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DispatchMessagesDeleted();
}

}  // namespace fcm

DEFINE_JNI(FcmBridge)
