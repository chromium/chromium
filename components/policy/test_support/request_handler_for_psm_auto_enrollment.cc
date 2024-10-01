// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_psm_auto_enrollment.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;
using RlweTestData =
    private_membership::rlwe::PrivateMembershipRlweClientRegressionTestData;

namespace policy {
namespace {

const RlweTestData::TestCase* FindOprfTestCase(
    const RlweTestData& test_data,
    const private_membership::rlwe::PrivateMembershipRlweOprfRequest& request) {
  for (const auto& test_case : test_data.test_cases()) {
    if (request.SerializeAsString() ==
        test_case.expected_oprf_request().SerializeAsString()) {
      return &test_case;
    }
  }
  return nullptr;
}

const RlweTestData::TestCase* FindQueryTestCase(
    const RlweTestData& test_data,
    const private_membership::rlwe::PrivateMembershipRlweQueryRequest&
        request) {
  for (const auto& test_case : test_data.test_cases()) {
    if (request.SerializeAsString() ==
        test_case.expected_query_request().SerializeAsString()) {
      return &test_case;
    }
  }
  return nullptr;
}

}  // namespace

// static
std::unique_ptr<RlweTestData>
RequestHandlerForPsmAutoEnrollment::LoadTestData() {
  base::FilePath src_root_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root_dir));
  const base::FilePath path_to_test_data =
      src_root_dir.AppendASCII("third_party")
          .AppendASCII("private_membership")
          .AppendASCII("src")
          .AppendASCII("internal")
          .AppendASCII("testing")
          .AppendASCII("regression_test_data")
          .AppendASCII("test_data.binarypb");

  base::ScopedAllowBlockingForTesting allow_blocking;

  auto test_data = std::make_unique<RlweTestData>();
  if (!base::PathExists(path_to_test_data)) {
    LOG(WARNING) << "Path to psm test data does not exist (this is expected in "
                    "tast tests, but not in unit tests: "
                 << path_to_test_data;
    return test_data;
  }

  std::string serialized_test_data;
  CHECK(base::ReadFileToString(path_to_test_data, &serialized_test_data));

  CHECK(test_data->ParseFromString(serialized_test_data));

  return test_data;
}

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
  if (!test_data_) {
    test_data_ = LoadTestData();
  }
  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);

  em::DeviceManagementResponse device_management_response;
  em::PrivateSetMembershipResponse* psm_response =
      device_management_response.mutable_private_set_membership_response();
  const auto& rlwe_request =
      device_management_request.private_set_membership_request().rlwe_request();
  if (rlwe_request.has_oprf_request()) {
    const auto* test_case =
        FindOprfTestCase(*test_data_, rlwe_request.oprf_request());
    if (!test_case) {
      return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                                "PSM RLWE OPRF request not as expected");
    }
    *psm_response->mutable_rlwe_response()->mutable_oprf_response() =
        test_case->oprf_response();
  } else if (rlwe_request.has_query_request()) {
    const auto* test_case =
        FindQueryTestCase(*test_data_, rlwe_request.query_request());
    if (!test_case) {
      return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                                "PSM RLWE query request not as expected");
    }
    *psm_response->mutable_rlwe_response()->mutable_query_response() =
        test_case->query_response();
  } else {
    return CreateHttpResponse(
        net::HTTP_BAD_REQUEST,
        "PSM RLWE oprf_request, or query_request fields must be filled");
  }

  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

}  // namespace policy
