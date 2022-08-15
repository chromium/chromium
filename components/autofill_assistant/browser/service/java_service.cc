// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/java_service.h"

#include <string>
#include <vector>

#include "chrome/android/features/autofill_assistant/test_support_jni_headers/AutofillAssistantTestService_jni.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "net/http/http_status_code.h"

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

void JavaService::GetScriptsForUrl(
    const GURL& url,
    const TriggerContext& trigger_context,
    ServiceRequestSender::ResponseCallback callback) {
  DCHECK(url.is_valid());
  JNIEnv* env = base::android::AttachCurrentThread();
  auto jresponse = Java_AutofillAssistantTestService_getScriptsForUrlNative(
      env, java_service_,
      base::android::ConvertUTF8ToJavaString(env, url.spec()));
  std::string response;
  base::android::JavaByteArrayToString(env, jresponse, &response);
  std::move(callback).Run(net::HTTP_OK, response,
                          ServiceRequestSender::ResponseInfo{});
}

void JavaService::GetActions(const std::string& script_path,
                             const GURL& url,
                             const TriggerContext& trigger_context,
                             const std::string& global_payload,
                             const std::string& script_payload,
                             ServiceRequestSender::ResponseCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto jresponse = Java_AutofillAssistantTestService_getActionsNative(
      env, java_service_,
      base::android::ConvertUTF8ToJavaString(env, script_path),
      base::android::ConvertUTF8ToJavaString(env, url.spec()),
      base::android::ToJavaByteArray(env, global_payload),
      base::android::ToJavaByteArray(env, script_payload));
  std::string response;
  base::android::JavaByteArrayToString(env, jresponse, &response);
  std::move(callback).Run(net::HTTP_OK, response,
                          ServiceRequestSender::ResponseInfo{});
}

void JavaService::GetNextActions(
    const TriggerContext& trigger_context,
    const std::string& previous_global_payload,
    const std::string& previous_script_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    const RoundtripTimingStats& timing_stats,
    const RoundtripNetworkStats& network_stats,
    ServiceRequestSender::ResponseCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto jprocessed_actions =
      Java_AutofillAssistantTestService_createProcessedActionsList(env);
  for (const auto& action : processed_actions) {
    std::string serialized_proto;
    bool success = action.SerializeToString(&serialized_proto);
    DCHECK(success);
    Java_AutofillAssistantTestService_addProcessedAction(
        env, jprocessed_actions,
        base::android::ToJavaByteArray(env, serialized_proto));
  }
  auto jresponse = Java_AutofillAssistantTestService_getNextActionsNative(
      env, java_service_,
      base::android::ToJavaByteArray(env, previous_global_payload),
      base::android::ToJavaByteArray(env, previous_script_payload),
      jprocessed_actions);
  std::string response;
  base::android::JavaByteArrayToString(env, jresponse, &response);
  std::move(callback).Run(net::HTTP_OK, response,
                          ServiceRequestSender::ResponseInfo{});
}

void JavaService::GetUserData(const CollectUserDataOptions& options,
                              uint64_t run_id,
                              const UserData* user_data,
                              ServiceRequestSender::ResponseCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto jresponse =
      Java_AutofillAssistantTestService_getUserDataNative(env, java_service_);
  std::string response;
  base::android::JavaByteArrayToString(env, jresponse, &response);
  std::move(callback).Run(net::HTTP_OK, response,
                          ServiceRequestSender::ResponseInfo{});
}

void JavaService::ReportProgress(
    const std::string& token,
    const std::string& payload,
    ServiceRequestSender::ResponseCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto jresponse = Java_AutofillAssistantTestService_reportProgressNative(
      env, java_service_);
  std::string response;
  base::android::JavaByteArrayToString(env, jresponse, &response);
  std::move(callback).Run(net::HTTP_OK, response,
                          ServiceRequestSender::ResponseInfo{});
}

}  // namespace autofill_assistant
