// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/java_service.h"

#include <string>
#include <vector>

#include "chrome/android/features/autofill_assistant/test_support_jni_headers/AutofillAssistantTestService_jni.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"

namespace autofill_assistant {

static jlong JNI_AutofillAssistantTestService_JavaServiceCreate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_service) {
  return reinterpret_cast<jlong>(new JavaService(java_service));
}

JavaService::JavaService(
    const base::android::JavaParamRef<jobject>& java_service)
    : java_service_(java_service) {}

JavaService::~JavaService() {}

void JavaService::GetScriptsForUrl(const GURL& url,
                                   const TriggerContext& trigger_context,
                                   ResponseCallback callback) {
  DCHECK(url.is_valid());
  JNIEnv* env = base::android::AttachCurrentThread();
  auto jresponse = Java_AutofillAssistantTestService_getScriptsForUrlNative(
      env, java_service_,
      base::android::ConvertUTF8ToJavaString(env, url.spec()));
  std::string response;
  base::android::JavaByteArrayToString(env, jresponse, &response);
  std::move(callback).Run(true, response);
}

void JavaService::GetActions(const std::string& script_path,
                             const GURL& url,
                             const TriggerContext& trigger_context,
                             const std::string& global_payload,
                             const std::string& script_payload,
                             ResponseCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto jresponse = Java_AutofillAssistantTestService_getActionsNative(
      env, java_service_,
      base::android::ConvertUTF8ToJavaString(env, script_path),
      base::android::ConvertUTF8ToJavaString(env, url.spec()),
      base::android::ToJavaByteArray(env, global_payload),
      base::android::ToJavaByteArray(env, script_payload));
  std::string response;
  base::android::JavaByteArrayToString(env, jresponse, &response);
  std::move(callback).Run(true, response);
}

void JavaService::GetNextActions(
    const TriggerContext& trigger_context,
    const std::string& previous_global_payload,
    const std::string& previous_script_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    ResponseCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto jresponse = Java_AutofillAssistantTestService_getNextActionsNative(
      env, java_service_,
      base::android::ToJavaByteArray(env, previous_global_payload),
      base::android::ToJavaByteArray(env, previous_script_payload));
  std::string response;
  base::android::JavaByteArrayToString(env, jresponse, &response);
  std::move(callback).Run(true, response);
}

}  // namespace autofill_assistant
