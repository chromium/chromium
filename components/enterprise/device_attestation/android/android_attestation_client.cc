// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/android/android_attestation_client.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/enterprise/device_attestation/android/jni_headers/AttestationBlobGenerator_jni.h"
#include "components/enterprise/device_attestation/android/jni_headers/BlobGenerationResult_jni.h"

using base::android::ScopedJavaLocalRef;

namespace enterprise {

namespace {

BlobGenerationResult GenerateAttestationBlobTask(std::string flow_name,
                                                 std::string request_hash,
                                                 std::string timestamp_hash,
                                                 std::string nonce_hash) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> generation_result =
      Java_AttestationBlobGenerator_generate(
          env, base::android::ConvertUTF8ToJavaString(env, flow_name),
          base::android::ConvertUTF8ToJavaString(env, request_hash),
          base::android::ConvertUTF8ToJavaString(env, timestamp_hash),
          base::android::ConvertUTF8ToJavaString(env, nonce_hash));

  return BlobGenerationResult{
      .attestation_blob = base::android::ConvertJavaStringToUTF8(
          env,
          Java_BlobGenerationResult_getAttestationBlob(env, generation_result)),
      .error_message = base::android::ConvertJavaStringToUTF8(
          env,
          Java_BlobGenerationResult_getErrorMessage(env, generation_result))};
}

}  // namespace

AndroidAttestationClient::AndroidAttestationClient() = default;
AndroidAttestationClient::~AndroidAttestationClient() = default;

void AndroidAttestationClient::GenerateAttestationBlob(
    std::string_view flow_name,
    std::string_view request_payload_hash,
    std::string_view timestamp_hash,
    std::string_view nonce_hash,
    base::OnceCallback<void(BlobGenerationResult)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&GenerateAttestationBlobTask, std::string(flow_name),
                     std::string(request_payload_hash),
                     std::string(timestamp_hash), std::string(nonce_hash)),
      std::move(callback));
}

}  // namespace enterprise
