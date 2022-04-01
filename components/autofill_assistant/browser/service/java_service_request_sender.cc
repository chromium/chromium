// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/java_service_request_sender.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/features/autofill_assistant/test_support_jni_headers/AutofillAssistantTestServiceRequestSender_jni.h"

namespace autofill_assistant {

static jlong JNI_AutofillAssistantTestServiceRequestSender_CreateNative(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_service_request_sender) {
  return reinterpret_cast<jlong>(
      new JavaServiceRequestSender(java_service_request_sender));
}

JavaServiceRequestSender::JavaServiceRequestSender(
    const base::android::JavaParamRef<jobject>& jservice_request_sender)
    : jservice_request_sender_(jservice_request_sender) {}
JavaServiceRequestSender::~JavaServiceRequestSender() = default;

void JavaServiceRequestSender::SendRequest(
    const GURL& url,
    const std::string& request_body,
    ServiceRequestSender::AuthMode auth_mode,
    ResponseCallback callback,
    RpcType rpc_type) {
  DCHECK(!callback_)
      << __func__
      << " invoked while still waiting for response to previous request";
  callback_ = std::move(callback);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillAssistantTestServiceRequestSender_sendRequest(
      env, jservice_request_sender_,
      base::android::ConvertUTF8ToJavaString(env, url.spec()),
      base::android::ToJavaByteArray(env, request_body));
}

void JavaServiceRequestSender::OnResponse(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint http_status,
    const base::android::JavaParamRef<jbyteArray>& jresponse) {
  DCHECK(callback_);
  std::string response;
  if (jresponse) {
    base::android::JavaByteArrayToString(env, jresponse, &response);
  }
  // Note: it is currently not necessary to mock the response info in ITs.
  std::move(callback_).Run(http_status, response, ResponseInfo{});
}

}  // namespace autofill_assistant
