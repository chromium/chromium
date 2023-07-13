// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/environment_integrity/android/integrity_service_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/environment_integrity/android/jni_headers/IntegrityServiceBridge_jni.h"
#include "content/public/browser/browser_thread.h"

namespace environment_integrity {

namespace {
const int32_t kIntegrityTimeoutMilliseconds = 5000;
}  // namespace

using base::android::AttachCurrentThread;

GetTokenResult::GetTokenResult() = default;
GetTokenResult::~GetTokenResult() = default;
GetTokenResult::GetTokenResult(GetTokenResult&&) = default;
GetTokenResult& GetTokenResult::operator=(GetTokenResult&&) = default;

bool IntegrityService::IsIntegrityAvailable() {
  return Java_IntegrityServiceBridge_isIntegrityAvailable(
      AttachCurrentThread());
}

void IntegrityService::CreateIntegrityHandle(CreateHandleCallback callback) {
  // Create a new callback allocation and pass the pointer to java.
  intptr_t callback_id =
      reinterpret_cast<intptr_t>(new CreateHandleCallback(std::move(callback)));
  Java_IntegrityServiceBridge_createHandle(AttachCurrentThread(), callback_id,
                                           kIntegrityTimeoutMilliseconds);
}

void IntegrityService::GetEnvironmentIntegrityToken(
    int64_t handle,
    const std::vector<uint8_t>& content_binding,
    GetTokenCallback callback) {
  // Create a new callback allocation and pass the pointer to java.
  intptr_t callback_id =
      reinterpret_cast<intptr_t>(new GetTokenCallback(std::move(callback)));
  JNIEnv* env = AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jbyteArray> binding_java =
      base::android::ToJavaByteArray(env, content_binding);
  Java_IntegrityServiceBridge_getIntegrityToken(
      env, callback_id, handle, binding_java, kIntegrityTimeoutMilliseconds);
}

void JNI_IntegrityServiceBridge_OnCreateHandleResult(
    JNIEnv* env,
    jlong callback_id,
    jint response_code,
    jlong handle,
    const base::android::JavaParamRef<jstring>& error_msg) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Recover the pointer, using a unique_ptr to clean up the memory when we're
  // done.
  std::unique_ptr<CreateHandleCallback> callback =
      base::WrapUnique<CreateHandleCallback>(
          reinterpret_cast<CreateHandleCallback*>(callback_id));

  HandleCreationResult result;
  result.response_code = IntegrityResponse(response_code);
  result.handle = handle;
  if (static_cast<IntegrityResponse>(response_code) !=
      IntegrityResponse::kSuccess) {
    result.error_message = base::android::ConvertJavaStringToUTF8(error_msg);
  }
  std::move(*callback).Run(std::move(result));
}

void JNI_IntegrityServiceBridge_OnGetIntegrityTokenResult(
    JNIEnv* env,
    jlong callback_id,
    jint response_code,
    const base::android::JavaParamRef<jbyteArray>& token,
    const base::android::JavaParamRef<jstring>& error_msg) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Recover the pointer, using a unique_ptr to clean up the memory when we're
  // done.
  std::unique_ptr<GetTokenCallback> callback =
      base::WrapUnique<GetTokenCallback>(
          reinterpret_cast<GetTokenCallback*>(callback_id));

  GetTokenResult result;
  result.response_code = IntegrityResponse(response_code);
  if (static_cast<IntegrityResponse>(response_code) ==
      IntegrityResponse::kSuccess) {
    base::android::JavaByteArrayToByteVector(env, token, &result.token);
  } else {
    result.error_message = base::android::ConvertJavaStringToUTF8(error_msg);
  }

  std::move(*callback).Run(std::move(result));
}
}  // namespace environment_integrity
