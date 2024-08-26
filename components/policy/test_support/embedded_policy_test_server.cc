// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/embedded_policy_test_server.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/remote_commands_state.h"
#include "components/policy/test_support/request_handler_for_check_user_account.h"
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/policy/proto/chrome_extension_policy.pb.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/request_handler_for_api_authorization.h"
#include "components/policy/test_support/request_handler_for_auto_enrollment.h"
#include "components/policy/test_support/request_handler_for_cert_upload.h"
#include "components/policy/test_support/request_handler_for_check_android_management.h"
#include "components/policy/test_support/request_handler_for_chrome_desktop_report.h"
#include "components/policy/test_support/request_handler_for_client_cert_provisioning.h"
#include "components/policy/test_support/request_handler_for_device_attribute_update.h"
#include "components/policy/test_support/request_handler_for_device_attribute_update_permission.h"
#include "components/policy/test_support/request_handler_for_device_initial_enrollment_state.h"
#include "components/policy/test_support/request_handler_for_device_state_retrieval.h"
#include "components/policy/test_support/request_handler_for_policy.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "components/policy/test_support/request_handler_for_psm_auto_enrollment.h"
#endif  // BUILDFLAG(IS_CHROMEOS)
#include "components/policy/test_support/remote_commands_state.h"
#include "components/policy/test_support/request_handler_for_register_browser.h"
#include "components/policy/test_support/request_handler_for_register_cert_based.h"
#include "components/policy/test_support/request_handler_for_register_device_and_user.h"
#include "components/policy/test_support/request_handler_for_remote_commands.h"
#include "components/policy/test_support/request_handler_for_status_upload.h"
#include "components/policy/test_support/request_handler_for_unregister.h"
#include "components/policy/test_support/request_handler_for_upload_euicc_info.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "crypto/sha2.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using ::net::test_server::EmbeddedTestServer;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

namespace policy {

namespace {

const char kExternalPolicyDataPath[] = "/externalpolicydata";
const char kExternalPolicyTypeParam[] = "policy_type";
const char kExternalEntityIdParam[] = "entity_id";

std::unique_ptr<HttpResponse> LogStatusAndReturn(
    GURL url,
    std::unique_ptr<HttpResponse> response) {
  if (!response) {
    return nullptr;
  }

  CustomHttpResponse* basic_response =
      static_cast<CustomHttpResponse*>(response.get());
  if (basic_response->code() == net::HTTP_OK) {
    LOG(INFO) << "Request succeeded: " << url;
  } else {
    LOG(ERROR) << "Request failed with error code " << basic_response->code()
               << " (" << basic_response->content() << "): " << url;
  }
  return response;
}

}  // namespace

const char kFakeDeviceToken[] = "fake_device_management_token";
const char kInvalidEnrollmentToken[] = "invalid_enrollment_token";

EmbeddedPolicyTestServer::RequestHandler::RequestHandler(
    EmbeddedPolicyTestServer* parent)
    : parent_(parent) {}

EmbeddedPolicyTestServer::RequestHandler::~RequestHandler() = default;

struct EmbeddedPolicyTestServer::ServerState {
  ClientStorage client_storage_;
  PolicyStorage policy_storage_;
  std::map<std::string, net::HttpStatusCode> configured_errors_map_;
};

ClientStorage* EmbeddedPolicyTestServer::client_storage() {
  return &server_state_->client_storage_;
}

PolicyStorage* EmbeddedPolicyTestServer::policy_storage() {
  return &server_state_->policy_storage_;
}

RemoteCommandsState* EmbeddedPolicyTestServer::remote_commands_state() {
  return remote_commands_state_.get();
}

EmbeddedPolicyTestServer::EmbeddedPolicyTestServer()
    : http_server_(EmbeddedTestServer::TYPE_HTTP) {
  remote_commands_state_ = std::make_unique<RemoteCommandsState>();
  ResetServerState();
  RegisterHandler(std::make_unique<RequestHandlerForApiAuthorization>(this));
  RegisterHandler(std::make_unique<RequestHandlerForAutoEnrollment>(this));
  RegisterHandler(std::make_unique<RequestHandlerForCertUpload>(this));
  RegisterHandler(
      std::make_unique<RequestHandlerForCheckAndroidManagement>(this));
  RegisterHandler(std::make_unique<RequestHandlerForCheckUserAccount>(this));
  RegisterHandler(std::make_unique<RequestHandlerForChromeDesktopReport>(this));
  RegisterHandler(
      std::make_unique<RequestHandlerForClientCertProvisioning>(this));
  RegisterHandler(
      std::make_unique<RequestHandlerForDeviceAttributeUpdate>(this));
  RegisterHandler(
      std::make_unique<RequestHandlerForDeviceAttributeUpdatePermission>(this));
  RegisterHandler(
      std::make_unique<RequestHandlerForDeviceInitialEnrollmentState>(this));
  RegisterHandler(
      std::make_unique<RequestHandlerForDeviceStateRetrieval>(this));
  RegisterHandler(std::make_unique<RequestHandlerForPolicy>(this));
#if BUILDFLAG(IS_CHROMEOS)
  RegisterHandler(std::make_unique<RequestHandlerForPsmAutoEnrollment>(this));
#endif  // BUILDFLAG(IS_CHROMEOS)
  RegisterHandler(std::make_unique<RequestHandlerForRegisterBrowser>(this));
  RegisterHandler(std::make_unique<RequestHandlerForRegisterPolicyAgent>(this));
  RegisterHandler(std::make_unique<RequestHandlerForRegisterCertBased>(this));
  RegisterHandler(
      std::make_unique<RequestHandlerForRegisterDeviceAndUser>(this));
  RegisterHandler(std::make_unique<RequestHandlerForRemoteCommands>(this));
  RegisterHandler(std::make_unique<RequestHandlerForStatusUpload>(this));
  RegisterHandler(std::make_unique<RequestHandlerForUnregister>(this));
  RegisterHandler(std::make_unique<RequestHandlerForUploadEuiccInfo>(this));

  http_server_.RegisterDefaultHandler(base::BindRepeating(
      &EmbeddedPolicyTestServer::HandleRequest, base::Unretained(this)));
}

EmbeddedPolicyTestServer::~EmbeddedPolicyTestServer() = default;

bool EmbeddedPolicyTestServer::Start() {
  return http_server_.Start();
}

GURL EmbeddedPolicyTestServer::GetServiceURL() const {
  return http_server_.GetURL("/device_management");
}

void EmbeddedPolicyTestServer::RegisterHandler(
    std::unique_ptr<EmbeddedPolicyTestServer::RequestHandler> request_handler) {
  request_handlers_[request_handler->RequestType()] =
      std::move(request_handler);
}

void EmbeddedPolicyTestServer::ConfigureRequestError(
    const std::string& request_type,
    net::HttpStatusCode error_code) {
  server_state_->configured_errors_map_.insert(
      std::pair<std::string, net::HttpStatusCode>(request_type, error_code));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void EmbeddedPolicyTestServer::UpdateExternalPolicy(
    const std::string& type,
    const std::string& entity_id,
    const std::string& raw_policy) {
  // Register raw policy to be served by external endpoint.
  policy_storage()->SetExternalPolicyPayload(type, entity_id, raw_policy);

  // Register proto policy with details on how to fetch the raw policy.
  GURL external_policy_url = http_server_.GetURL(kExternalPolicyDataPath);
  external_policy_url = net::AppendOrReplaceQueryParameter(
      external_policy_url, kExternalPolicyTypeParam, type);
  external_policy_url = net::AppendOrReplaceQueryParameter(
      external_policy_url, kExternalEntityIdParam, entity_id);

  enterprise_management::ExternalPolicyData external_policy_data;
  external_policy_data.set_download_url(external_policy_url.spec());
  external_policy_data.set_secure_hash(crypto::SHA256HashString(raw_policy));
  policy_storage()->SetPolicyPayload(type, entity_id,
                                     external_policy_data.SerializeAsString());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

std::unique_ptr<HttpResponse> EmbeddedPolicyTestServer::HandleRequest(
    const HttpRequest& request) {
  GURL url = request.GetURL();
  LOG(INFO) << "Request URL: " << url;

  if (url.path() == kExternalPolicyDataPath) {
    return HandleExternalPolicyDataRequest(url);
  }

  std::string request_type = KeyValueFromUrl(url, dm_protocol::kParamRequest);

  auto it_errors = server_state_->configured_errors_map_.find(request_type);
  if (it_errors != server_state_->configured_errors_map_.end()) {
    return LogStatusAndReturn(
        url, CreateHttpResponse(it_errors->second, "Preconfigured error"));
  }

  auto it_handlers = request_handlers_.find(request_type);
  if (it_handlers == request_handlers_.end()) {
    LOG(ERROR) << "No request handler for: " << url;
    return nullptr;
  }

  if (!MeetsServerSideRequirements(url)) {
    return LogStatusAndReturn(
        url, CreateHttpResponse(
                 net::HTTP_BAD_REQUEST,
                 "URL must define device type, app type, and device id."));
  }

  return LogStatusAndReturn(url, it_handlers->second->HandleRequest(request));
}

std::unique_ptr<HttpResponse>
EmbeddedPolicyTestServer::HandleExternalPolicyDataRequest(const GURL& url) {
  DCHECK_EQ(url.path(), kExternalPolicyDataPath);
  std::string policy_type = KeyValueFromUrl(url, kExternalPolicyTypeParam);
  std::string entity_id = KeyValueFromUrl(url, kExternalEntityIdParam);
  std::string policy_payload =
      policy_storage()->GetExternalPolicyPayload(policy_type, entity_id);
  std::unique_ptr<HttpResponse> response;
  if (policy_payload.empty()) {
    response = CreateHttpResponse(
        net::HTTP_NOT_FOUND,
        "No external policy payload for specified policy type and entity ID");
  } else {
    response = CreateHttpResponse(net::HTTP_OK, policy_payload);
  }
  return LogStatusAndReturn(url, std::move(response));
}

void EmbeddedPolicyTestServer::ResetServerState() {
  server_state_ = std::make_unique<ServerState>();
}

}  // namespace policy
