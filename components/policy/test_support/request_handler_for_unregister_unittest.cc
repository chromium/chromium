// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_unregister.h"

#include <utility>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kDeviceToken[] = "fake_device_token";
constexpr char kNonExistingDeviceToken[] = "non_existing_device_token";

}  // namespace

class RequestHandlerForUnregisterTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  RequestHandlerForUnregisterTest() = default;
  ~RequestHandlerForUnregisterTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestUnregister);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);

    ClientStorage::ClientInfo client_info;
    client_info.device_id = kDeviceId;
    client_info.device_token = kDeviceToken;
    client_storage()->RegisterClient(client_info);
  }
};

TEST_F(RequestHandlerForUnregisterTest, HandleRequest_NoDeviceToken) {
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_UNAUTHORIZED);
  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 1u);
}

TEST_F(RequestHandlerForUnregisterTest, HandleRequest_ClientNotFound) {
  SetDeviceTokenHeader(kNonExistingDeviceToken);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_UNAUTHORIZED);
  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 1u);
}

TEST_F(RequestHandlerForUnregisterTest, HandleRequest_DeleteClient) {
  SetDeviceTokenHeader(kDeviceToken);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  EXPECT_EQ(client_storage()->GetNumberOfRegisteredClients(), 0u);
}

}  // namespace policy
