// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/simple_request_handler.h"

#include <memory>
#include <string>

#include "components/policy/proto/device_management_backend.pb.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

TEST(SimpleRequestHandlerTest, RequestType) {
  em::DeviceManagementResponse response;
  SimpleRequestHandler handler(nullptr, "test_request_type", response);
  EXPECT_EQ(handler.RequestType(), "test_request_type");
}

TEST(SimpleRequestHandlerTest, HandleRequest) {
  em::DeviceManagementResponse response;
  response.mutable_cert_upload_response();
  SimpleRequestHandler handler(nullptr, "test_request_type", response);

  net::test_server::HttpRequest request;
  std::unique_ptr<net::test_server::HttpResponse> http_response =
      handler.HandleRequest(request);

  ASSERT_TRUE(http_response);
  auto* basic_response =
      static_cast<net::test_server::BasicHttpResponse*>(http_response.get());
  EXPECT_EQ(basic_response->code(), net::HTTP_OK);

  em::DeviceManagementResponse parsed_response;
  EXPECT_TRUE(
      parsed_response.ParseFromString(std::string(basic_response->content())));
  EXPECT_TRUE(parsed_response.has_cert_upload_response());
}

}  // namespace policy
