// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/failing_request_handler.h"

#include <utility>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";

}  // namespace

class FailingRequestHandlerTest : public EmbeddedPolicyTestServerTestBase {
 public:
  FailingRequestHandlerTest() = default;
  ~FailingRequestHandlerTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    SetRequestTypeParam(dm_protocol::kValueRequestRegister);
    SetAppType(dm_protocol::kValueAppType);
    SetDeviceIdParam(kDeviceId);
    SetDeviceType(dm_protocol::kValueDeviceType);
  }
};

TEST_F(FailingRequestHandlerTest, HandleRequest) {
  test_server()->ConfigureRequestError(dm_protocol::kValueRequestRegister,
                                       net::HTTP_METHOD_NOT_ALLOWED);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_METHOD_NOT_ALLOWED);
}

}  // namespace policy
