// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handlers_for_gcm.h"

#include <string>

#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class RequestHandlerForGcmCheckinTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();
    SetURL(test_server()->GetGcmCheckinURL());
  }
};

TEST_F(RequestHandlerForGcmCheckinTest, InitialCheckinSucceeds) {
  checkin_proto::AndroidCheckinRequest request;
  request.set_id(0);
  request.mutable_checkin()->set_type(checkin_proto::DEVICE_CHROME_BROWSER);

  SetPayload(request.SerializeAsString());
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  checkin_proto::AndroidCheckinResponse response;
  ASSERT_TRUE(response.ParseFromString(GetResponseBody()));
  EXPECT_TRUE(response.stats_ok());
  EXPECT_NE(response.android_id(), 0u);
  EXPECT_NE(response.security_token(), 0u);
}

TEST_F(RequestHandlerForGcmCheckinTest, PreservesExistingAndroidId) {
  checkin_proto::AndroidCheckinRequest request;
  request.set_id(99999);
  request.set_security_token(88888);
  request.mutable_checkin()->set_type(checkin_proto::DEVICE_CHROME_BROWSER);

  SetPayload(request.SerializeAsString());
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  checkin_proto::AndroidCheckinResponse response;
  ASSERT_TRUE(response.ParseFromString(GetResponseBody()));
  EXPECT_TRUE(response.stats_ok());
  EXPECT_EQ(response.android_id(), 99999u);
  EXPECT_EQ(response.security_token(), 88888u);
}

TEST_F(RequestHandlerForGcmCheckinTest, InvalidPayloadReturnsBadRequest) {
  SetPayload("invalid payload");
  StartRequestAndWait();
  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

class RequestHandlerForGcmRegisterTest
    : public EmbeddedPolicyTestServerTestBase {
 protected:
  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();
    SetURL(test_server()->GetGcmRegistrationURL());
  }
};

TEST_F(RequestHandlerForGcmRegisterTest, RegisterWithAuthHeaderSucceeds) {
  SetHeader(net::HttpRequestHeaders::kAuthorization, "AidLogin 54321:98765");
  SetPayload("app=com.google.chrome.fcm.invalidations-245350905893");
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  EXPECT_EQ(GetResponseBody(),
            "token=fake_fcm_token_54321_com.google.chrome.fcm.invalidations-"
            "245350905893");
}

TEST_F(RequestHandlerForGcmRegisterTest, RegisterWithFormBodySucceeds) {
  SetPayload("device=67890&app=test_app");
  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  EXPECT_EQ(GetResponseBody(), "token=fake_fcm_token_67890_test_app");
}

TEST_F(RequestHandlerForGcmRegisterTest, RegisterDefaultFallbackSucceeds) {
  StartRequestAndWait();
  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  EXPECT_EQ(GetResponseBody(), "token=fake_fcm_token_12345");
}

}  // namespace policy
