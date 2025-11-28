// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/android/attestation_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "base/threading/scoped_blocking_call.h"
#include "crypto/hash.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/enterprise/device_attestation/android/jni_headers/AttestationBlobGenerator_jni.h"
#include "components/enterprise/device_attestation/android/jni_headers/BlobGenerationResult_jni.h"

using base::android::ScopedJavaLocalRef;

namespace {

// The report request should be hashed in the format of:
// "<Serialized request>.<report timestamp>.<nonce>""
constexpr char kReportRequestHashKey[] = "%s.%s.%s";

std::string GetHashString(std::string_view payload) {
  return base::Base64Encode(crypto::hash::Sha256(payload));
}

}  // namespace

namespace enterprise {

// TODO(crbug.com/445677557): Add C++ test coverage once internal logic is
// implemented, if Java test coverage is insufficient.
BlobGenerationResult GenerateAttestationBlob(std::string_view flow_name,
                                             std::string_view request_payload,
                                             std::string_view timestamp,
                                             std::string_view nonce) {
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    JNIEnv* env = base::android::AttachCurrentThread();
    ScopedJavaLocalRef<jobject> generation_result =
        Java_AttestationBlobGenerator_generate(
            env, base::android::ConvertUTF8ToJavaString(env, flow_name),
            base::android::ConvertUTF8ToJavaString(
                env,
                GetHashString(base::StringPrintf(
                    kReportRequestHashKey, request_payload, timestamp, nonce))),
            base::android::ConvertUTF8ToJavaString(env,
                                                   GetHashString(timestamp)),
            base::android::ConvertUTF8ToJavaString(env, GetHashString(nonce)));

    return {base::android::ConvertJavaStringToUTF8(
                env, Java_BlobGenerationResult_getAttestationBlob(
                         env, generation_result)),
            base::android::ConvertJavaStringToUTF8(
                env, Java_BlobGenerationResult_getErrorMessage(
                         env, generation_result))};
  }
}

}  // namespace enterprise

DEFINE_JNI(AttestationBlobGenerator)
DEFINE_JNI(BlobGenerationResult)
