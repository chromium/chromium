// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/embedded_policy_test_server_test_base.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"

using enterprise_management::DeviceManagementResponse;

namespace policy {

EmbeddedPolicyTestServerTestBase::EmbeddedPolicyTestServerTestBase()
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
      url_loader_factory_(
          base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {}

EmbeddedPolicyTestServerTestBase::~EmbeddedPolicyTestServerTestBase() = default;

void EmbeddedPolicyTestServerTestBase::SetUp() {
  test_server_.Start();

  resource_request_ = std::make_unique<network::ResourceRequest>();
  resource_request_->method = net::HttpRequestHeaders::kPostMethod;
  resource_request_->url = test_server_.GetServiceURL();
}

void EmbeddedPolicyTestServerTestBase::AddQueryParam(const std::string& key,
                                                     const std::string& value) {
  CHECK(resource_request_);
  CHECK(!key.empty());
  CHECK(!value.empty());

  resource_request_->url =
      net::AppendQueryParameter(resource_request_->url, key, value);
}

void EmbeddedPolicyTestServerTestBase::SetURL(const GURL& url) {
  resource_request_->url = url;
}

void EmbeddedPolicyTestServerTestBase::SetMethod(const std::string& method) {
  resource_request_->method = method;
}

void EmbeddedPolicyTestServerTestBase::SetAppType(const std::string& app_type) {
  AddQueryParam(dm_protocol::kParamAppType, app_type);
}

void EmbeddedPolicyTestServerTestBase::SetDeviceIdParam(
    const std::string& device_id) {
  AddQueryParam(dm_protocol::kParamDeviceID, device_id);
}

void EmbeddedPolicyTestServerTestBase::SetDeviceType(
    const std::string& device_type) {
  AddQueryParam(dm_protocol::kParamDeviceType, device_type);
}

void EmbeddedPolicyTestServerTestBase::SetOAuthToken(
    const std::string& oauth_token) {
  AddQueryParam(dm_protocol::kParamOAuthToken, oauth_token);
}

void EmbeddedPolicyTestServerTestBase::SetRequestTypeParam(
    const std::string& request_type) {
  AddQueryParam(dm_protocol::kParamRequest, request_type);
}

void EmbeddedPolicyTestServerTestBase::SetEnrollmentTokenHeader(
    const std::string& enrollment_token) {
  CHECK(resource_request_);
  CHECK(!enrollment_token.empty());

  resource_request_->headers.SetHeader(
      dm_protocol::kAuthHeader,
      std::string(dm_protocol::kEnrollmentTokenAuthHeaderPrefix)
          .append(enrollment_token));
}

void EmbeddedPolicyTestServerTestBase::SetDeviceTokenHeader(
    const std::string& device_token) {
  CHECK(resource_request_);
  CHECK(!device_token.empty());

  resource_request_->headers.SetHeader(
      dm_protocol::kAuthHeader,
      std::string(dm_protocol::kDMTokenAuthHeaderPrefix).append(device_token));
}

void EmbeddedPolicyTestServerTestBase::SetGoogleLoginTokenHeader(
    const std::string& user_email) {
  CHECK(resource_request_);
  CHECK(!user_email.empty());

  resource_request_->headers.SetHeader(
      dm_protocol::kAuthHeader,
      std::string(dm_protocol::kServiceTokenAuthHeaderPrefix)
          .append(user_email));
}

void EmbeddedPolicyTestServerTestBase::SetPayload(
    const enterprise_management::DeviceManagementRequest&
        device_management_request) {
  CHECK(payload_.empty());

  payload_ = device_management_request.SerializeAsString();
}

void EmbeddedPolicyTestServerTestBase::StartRequestAndWait() {
  CHECK(resource_request_);
  CHECK(!url_loader_);

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request_),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);

  if (!payload_.empty())
    url_loader_->AttachStringForUpload(payload_, "application/protobuf");

  base::RunLoop run_loop;
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&EmbeddedPolicyTestServerTestBase::DownloadedToString,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
}

void EmbeddedPolicyTestServerTestBase::DownloadedToString(
    base::OnceClosure callback,
    std::unique_ptr<std::string> response_body) {
  CHECK(!done_);
  CHECK(!response_body_);
  CHECK(callback);

  response_body_ = std::move(response_body);
  done_ = true;
  std::move(callback).Run();
}

int EmbeddedPolicyTestServerTestBase::GetResponseCode() const {
  CHECK(done_);
  CHECK(url_loader_->ResponseInfo());
  CHECK(url_loader_->ResponseInfo()->headers);

  return url_loader_->ResponseInfo()->headers->response_code();
}

bool EmbeddedPolicyTestServerTestBase::HasResponseBody() const {
  return response_body_ != nullptr;
}

std::string EmbeddedPolicyTestServerTestBase::GetResponseBody() const {
  CHECK(response_body_);

  return *response_body_;
}

DeviceManagementResponse
EmbeddedPolicyTestServerTestBase::GetDeviceManagementResponse() const {
  CHECK(response_body_);

  DeviceManagementResponse device_management_response;
  device_management_response.ParseFromString(*response_body_);
  return device_management_response;
}

}  // namespace policy
