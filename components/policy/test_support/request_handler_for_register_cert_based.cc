// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_register_cert_based.h"

#include <set>
#include <string>

#include "base/notreached.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/request_handler_for_register_device_and_user.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

RequestHandlerForRegisterCertBased::RequestHandlerForRegisterCertBased(
    EmbeddedPolicyTestServer* parent)
    : RequestHandlerForRegisterDeviceAndUser(parent) {}

RequestHandlerForRegisterCertBased::~RequestHandlerForRegisterCertBased() =
    default;

std::string RequestHandlerForRegisterCertBased::RequestType() {
  return dm_protocol::kValueRequestCertBasedRegister;
}

std::unique_ptr<HttpResponse> RequestHandlerForRegisterCertBased::HandleRequest(
    const HttpRequest& request) {
  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);
  const em::SignedData& signed_req =
      device_management_request.certificate_based_register_request()
          .signed_request();
  em::CertificateBasedDeviceRegistrationData parsed_req;
  std::string data = signed_req.data().substr(
      0, signed_req.data().size() - signed_req.extra_data_bytes());
  if (!parsed_req.ParseFromString(data))
    return CreateHttpResponse(net::HTTP_BAD_REQUEST, "Invalid request");
  if (parsed_req.certificate_type() !=
      em::CertificateBasedDeviceRegistrationData::
          ENTERPRISE_ENROLLMENT_CERTIFICATE) {
    return CreateHttpResponse(net::HTTP_FORBIDDEN,
                              "Invalid certificate type for registration");
  }
  const em::DeviceRegisterRequest& register_request =
      parsed_req.device_register_request();

  return RegisterDeviceAndSendResponse(request, register_request, "");
}

}  // namespace policy
