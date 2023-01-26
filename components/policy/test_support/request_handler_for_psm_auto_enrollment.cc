// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_psm_auto_enrollment.h"

#include "base/containers/contains.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr const char* kPsmMembershipEncryptedTestIds[] = {
    "54455354/111111",  // Brand code "TEST" (as hex), serial number "111111".
};

}  // namespace

RequestHandlerForPsmAutoEnrollment::RequestHandlerForPsmAutoEnrollment(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForPsmAutoEnrollment::~RequestHandlerForPsmAutoEnrollment() =
    default;

std::string RequestHandlerForPsmAutoEnrollment::RequestType() {
  return dm_protocol::kValueRequestPsmHasDeviceState;
}

std::unique_ptr<HttpResponse> RequestHandlerForPsmAutoEnrollment::HandleRequest(
    const HttpRequest& request) {
  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);
  const em::PrivateSetMembershipRequest& psm_request =
      device_management_request.private_set_membership_request();

  em::DeviceManagementResponse device_management_response;
  em::PrivateSetMembershipResponse* psm_response =
      device_management_response.mutable_private_set_membership_response();
  const auto& rlwe_request = psm_request.rlwe_request();
  if (rlwe_request.has_oprf_request()) {
    if (rlwe_request.oprf_request().encrypted_ids_size() == 0) {
      return CreateHttpResponse(
          net::HTTP_BAD_REQUEST,
          "PSM RLWE OPRF request must contain encrypted_ids field");
    }
    psm_response->mutable_rlwe_response()
        ->mutable_oprf_response()
        ->add_doubly_encrypted_ids()
        ->set_queried_encrypted_id(
            rlwe_request.oprf_request().encrypted_ids(0));
  } else if (rlwe_request.has_query_request()) {
    if (rlwe_request.query_request().queries_size() == 0) {
      return CreateHttpResponse(
          net::HTTP_BAD_REQUEST,
          "PSM RLWE query request must contain queries field");
    }
    auto* pir_response = psm_response->mutable_rlwe_response()
                             ->mutable_query_response()
                             ->add_pir_responses();
    const auto& encrypted_id =
        rlwe_request.query_request().queries(0).queried_encrypted_id();
    pir_response->set_queried_encrypted_id(encrypted_id);
    pir_response->mutable_pir_response()->set_plaintext_entry_size(
        base::Contains(kPsmMembershipEncryptedTestIds, encrypted_id)
            ? kPirResponseHasMembership
            : kPirResponseHasNoMembership);
  } else {
    return CreateHttpResponse(
        net::HTTP_BAD_REQUEST,
        "PSM RLWE oprf_request, or query_request fields must be filled");
  }

  return CreateHttpResponse(net::HTTP_OK,
                            device_management_response.SerializeAsString());
}

}  // namespace policy
