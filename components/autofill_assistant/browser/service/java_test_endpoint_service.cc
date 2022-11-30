// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service/java_test_endpoint_service.h"

#include <string>
#include <vector>

#include "chrome/android/features/autofill_assistant/test_support_jni_headers/AutofillAssistantTestEndpointService_jni.h"
#include "components/autofill_assistant/browser/client.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/server_url_fetcher.h"
#include "components/autofill_assistant/browser/service/service_impl.h"
#include "content/public/browser/web_contents.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "net/http/http_status_code.h"

namespace autofill_assistant {

static jlong JNI_AutofillAssistantTestEndpointService_JavaServiceCreate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_object,
    jlong jnative_client,
    const base::android::JavaParamRef<jstring>& jendpoint_url) {
  std::string endpoint_url;
  if (jendpoint_url) {
    endpoint_url = base::android::ConvertJavaStringToUTF8(env, jendpoint_url);
  }
  if (endpoint_url.empty()) {
    NOTREACHED() << "Missing endpoint parameter for test endpoint service.";
    return 0;
  }
  if (!GURL(endpoint_url).is_valid()) {
    NOTREACHED() << "Invalid endpoint URL: " << endpoint_url;
    return 0;
  }
  ServerUrlFetcher server_url_fetcher = {GURL(endpoint_url)};
  if (server_url_fetcher.IsProdEndpoint()) {
    NOTREACHED() << "Tests cannot be run against the prod endpoint.";
    return 0;
  }

  Client* client =
      static_cast<Client*>(reinterpret_cast<void*>(jnative_client));
  if (!client) {
    NOTREACHED();
    return 0;
  }

  return reinterpret_cast<jlong>(new JavaTestEndpointService(
      ServiceImpl::Create(client->GetWebContents()->GetBrowserContext(), client,
                          server_url_fetcher)));
}

JavaTestEndpointService::JavaTestEndpointService(
    std::unique_ptr<Service> service_impl)
    : service_impl_(std::move(service_impl)) {}

JavaTestEndpointService::~JavaTestEndpointService() {}

void JavaTestEndpointService::GetScriptsForUrl(
    const GURL& url,
    const TriggerContext& trigger_context,
    ServiceRequestSender::ResponseCallback callback) {
  service_impl_->GetScriptsForUrl(
      url, trigger_context,
      base::BindOnce(&JavaTestEndpointService::OnGetScriptsForUrl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void JavaTestEndpointService::OnGetScriptsForUrl(
    ServiceRequestSender::ResponseCallback callback,
    int http_status,
    const std::string& response,
    const ServiceRequestSender::ResponseInfo& response_info) {
  if (http_status != ::net::HTTP_OK) {
    std::move(callback).Run(http_status, response, response_info);
    return;
  }

  SupportsScriptResponseProto proto_response;
  if (!proto_response.ParseFromString(response)) {
    std::move(callback).Run(http_status, response, response_info);
    return;
  }

  auto* integration_test_settings = proto_response.mutable_client_settings()
                                        ->mutable_integration_test_settings();
  integration_test_settings->set_disable_header_animations(true);
  integration_test_settings->set_disable_carousel_change_animations(true);

  std::string modified_response;
  proto_response.SerializeToString(&modified_response);
  std::move(callback).Run(http_status, modified_response, response_info);
}

void JavaTestEndpointService::GetActions(
    const std::string& script_path,
    const GURL& url,
    const TriggerContext& trigger_context,
    const std::string& global_payload,
    const std::string& script_payload,
    ServiceRequestSender::ResponseCallback callback) {
  service_impl_->GetActions(script_path, url, trigger_context, global_payload,
                            script_payload, std::move(callback));
}

void JavaTestEndpointService::GetNextActions(
    const TriggerContext& trigger_context,
    const std::string& previous_global_payload,
    const std::string& previous_script_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    const RoundtripTimingStats& timing_stats,
    const RoundtripNetworkStats& network_stats,
    ServiceRequestSender::ResponseCallback callback) {
  service_impl_->GetNextActions(
      trigger_context, previous_global_payload, previous_script_payload,
      processed_actions, timing_stats, network_stats, std::move(callback));
}

void JavaTestEndpointService::GetUserData(
    const CollectUserDataOptions& options,
    uint64_t run_id,
    const UserData* user_data,
    ServiceRequestSender::ResponseCallback callback) {
  service_impl_->GetUserData(options, run_id, user_data, std::move(callback));
}

void JavaTestEndpointService::ReportProgress(
    const std::string& token,
    const std::string& payload,
    ServiceRequestSender::ResponseCallback callback) {
  service_impl_->ReportProgress(token, payload, std::move(callback));
}

}  // namespace autofill_assistant
