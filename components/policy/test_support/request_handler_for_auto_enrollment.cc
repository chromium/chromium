// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_auto_enrollment.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

namespace {

void AddHashes(const std::vector<std::string>& hashes,
               em::DeviceAutoEnrollmentResponse* response) {
  for (const std::string& hash : hashes) {
    *response->add_hashes() = hash;
  }
}

}  // namespace

RequestHandlerForAutoEnrollment::RequestHandlerForAutoEnrollment(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForAutoEnrollment::~RequestHandlerForAutoEnrollment() = default;

std::string RequestHandlerForAutoEnrollment::RequestType() {
  return dm_protocol::kValueRequestAutoEnrollment;
}

std::unique_ptr<HttpResponse> RequestHandlerForAutoEnrollment::HandleRequest(
    const HttpRequest& request) {
  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);
  const em::DeviceAutoEnrollmentRequest& enrollment_request =
      device_management_request.auto_enrollment_request();

  em::DeviceManagementResponse device_management_response;
  em::DeviceAutoEnrollmentResponse* enrollment_response =
      device_management_response.mutable_auto_enrollment_response();
  switch (enrollment_request.modulus()) {
    case 1:
      if (enrollment_request.enrollment_check_type() ==
          enterprise_management::DeviceAutoEnrollmentRequest::
              ENROLLMENT_CHECK_TYPE_FRE) {
        AddHashes(
            client_storage()->GetMatchingStateKeyHashes(
                enrollment_request.modulus(), enrollment_request.remainder()),
            enrollment_response);
      } else if (enrollment_request.enrollment_check_type() ==
                 enterprise_management::DeviceAutoEnrollmentRequest::
                     ENROLLMENT_CHECK_TYPE_FORCED_ENROLLMENT) {
        AddHashes(
            policy_storage()->GetMatchingSerialHashes(
                enrollment_request.modulus(), enrollment_request.remainder()),
            enrollment_response);
      }
      break;
    case 32:
      enrollment_response->set_expected_modulus(1);
      break;
  }

  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

}  // namespace policy
