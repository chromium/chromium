// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/embedded_policy_test_server.h"

#include <string_view>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/policy/proto/chrome_extension_policy.pb.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kFakeDeviceId[] = "fake_device_id";
constexpr char kFakeRequestType[] = "fake_request_type";
constexpr char kInvalidRequestType[] = "invalid_request_type";
constexpr char kResponseBodyYay[] = "Yay!!!";
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
constexpr char kFakeExtensionId[] = "fake_extension_id";
constexpr char kRawPolicyPayload[] = R"({"foo": "bar"})";
constexpr std::string_view kSHA256HashForRawPolicyPayload(
    "\x42\x6f\xc0\x4f\x04\xbf\x8f\xdb\x58\x31\xdc\x37\xbb\xb6\xdc\xf7\x0f\x63"
    "\xa3\x7e\x05\xa6\x8c\x6e\xa5\xf6\x3e\x85\xae\x57\x93\x76",
    32);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

class FakeRequestHandler : public EmbeddedPolicyTestServer::RequestHandler {
 public:
  FakeRequestHandler() : RequestHandler(nullptr) {}
  ~FakeRequestHandler() override = default;

  std::string RequestType() override { return kFakeRequestType; }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) override {
    return CreateHttpResponse(net::HTTP_OK, kResponseBodyYay);
  }
};

}  // namespace

class EmbeddedPolicyTestServerTest : public EmbeddedPolicyTestServerTestBase {
 public:
  EmbeddedPolicyTestServerTest() = default;
  ~EmbeddedPolicyTestServerTest() override = default;

  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();

    test_server()->RegisterHandler(std::make_unique<FakeRequestHandler>());
  }
};

TEST_F(EmbeddedPolicyTestServerTest, HandleRequest_InvalidRequestType) {
  SetRequestTypeParam(kInvalidRequestType);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_NOT_FOUND);
}

TEST_F(EmbeddedPolicyTestServerTest, HandleRequest_Success) {
  SetRequestTypeParam(kFakeRequestType);
  SetAppType(dm_protocol::kValueAppType);
  SetDeviceIdParam(kFakeDeviceId);
  SetDeviceType(dm_protocol::kValueDeviceType);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  EXPECT_EQ(kResponseBodyYay, GetResponseBody());
}

TEST_F(EmbeddedPolicyTestServerTest, HandleRequest_MissingAppType) {
  SetRequestTypeParam(kFakeRequestType);
  SetDeviceIdParam(kFakeDeviceId);
  SetDeviceType(dm_protocol::kValueDeviceType);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

TEST_F(EmbeddedPolicyTestServerTest, HandleRequest_MissingDeviceId) {
  SetRequestTypeParam(kFakeRequestType);
  SetAppType(dm_protocol::kValueAppType);
  SetDeviceType(dm_protocol::kValueDeviceType);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

TEST_F(EmbeddedPolicyTestServerTest, HandleRequest_MissingDeviceType) {
  SetRequestTypeParam(kFakeRequestType);
  SetAppType(dm_protocol::kValueAppType);
  SetDeviceIdParam(kFakeDeviceId);

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_BAD_REQUEST);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(EmbeddedPolicyTestServerTest, HandleRequest_PolicyViaExternalEndpoint) {
  test_server()->UpdateExternalPolicy(dm_protocol::kChromeExtensionPolicyType,
                                      kFakeExtensionId, kRawPolicyPayload);

  std::string policy_data = test_server()->policy_storage()->GetPolicyPayload(
      dm_protocol::kChromeExtensionPolicyType, kFakeExtensionId);
  ASSERT_FALSE(policy_data.empty());
  enterprise_management::ExternalPolicyData data;
  ASSERT_TRUE(data.ParseFromString(policy_data));
  EXPECT_EQ(data.secure_hash(), kSHA256HashForRawPolicyPayload);
  ASSERT_TRUE(data.has_download_url());

  SetMethod(net::HttpRequestHeaders::kGetMethod);
  SetURL(GURL(data.download_url()));

  StartRequestAndWait();

  EXPECT_EQ(GetResponseCode(), net::HTTP_OK);
  ASSERT_TRUE(HasResponseBody());
  EXPECT_EQ(GetResponseBody(), kRawPolicyPayload);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace policy
