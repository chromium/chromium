// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/test_server_helpers.h"

#include <utility>
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/re2/src/re2/re2.h"

namespace policy {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

std::string KeyValueFromUrl(GURL url, const std::string& key) {
  std::string value;
  return net::GetValueForKeyInQuery(url, key, &value) ? value : std::string();
}

bool MeetsServerSideRequirements(GURL url) {
  std::string device_id = KeyValueFromUrl(url, dm_protocol::kParamDeviceID);
  return KeyValueFromUrl(url, dm_protocol::kParamDeviceType) ==
             dm_protocol::kValueDeviceType &&
         KeyValueFromUrl(url, dm_protocol::kParamAppType) ==
             dm_protocol::kValueAppType &&
         !device_id.empty() && device_id.size() <= 64;
}

bool GetTokenFromAuthorization(const HttpRequest& request,
                               const std::string& token_header_prefix,
                               std::string* out) {
  auto authorization = request.headers.find(dm_protocol::kAuthHeader);
  return authorization != request.headers.end() &&
         re2::RE2::FullMatch(authorization->second,
                             token_header_prefix + "(.+)", out);
}

bool GetEnrollmentTokenFromRequest(const HttpRequest& request,
                                   std::string* out) {
  return GetTokenFromAuthorization(
      request, dm_protocol::kEnrollmentTokenAuthHeaderPrefix, out);
}

bool GetDeviceTokenFromRequest(const HttpRequest& request, std::string* out) {
  return GetTokenFromAuthorization(request,
                                   dm_protocol::kDMTokenAuthHeaderPrefix, out);
}

bool GetGoogleLoginFromRequest(const net::test_server::HttpRequest& request,
                               std::string* out) {
  return net::GetValueForKeyInQuery(request.GetURL(), "oauth_token", out) ||
         GetTokenFromAuthorization(
             request, dm_protocol::kServiceTokenAuthHeaderPrefix, out);
}

std::unique_ptr<HttpResponse> CreateHttpResponse(net::HttpStatusCode code,
                                                 const std::string& content) {
  auto response = std::make_unique<BasicHttpResponse>();
  response->set_content_type("text/plain");
  response->set_code(code);
  response->set_content(content);
  return response;
}

}  // namespace policy
