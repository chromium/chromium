// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/test_server_helpers.h"

#include <ranges>
#include <utility>

#include "base/ranges/algorithm.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/re2/src/re2/re2.h"

namespace policy {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace {

// C++ does not offer a mechanism to check if a given status code is present in
// net::HttpStatusCode enum. To allow distinguishing standard HTTP status code
// from custom ones, we define this array that will contain all standard codes.
constexpr net::HttpStatusCode kStandardHttpStatusCodes[] = {
#define HTTP_STATUS_ENUM_VALUE(label, code, reason) net::HttpStatusCode(code),
#include "net/http/http_status_code_list.h"
#undef HTTP_STATUS_ENUM_VALUE
};

std::unique_ptr<HttpResponse> CreateHttpResponse(
    net::HttpStatusCode code,
    const std::string& content,
    const std::string& content_type) {
  auto response = std::make_unique<CustomHttpResponse>();
  response->set_content_type(content_type);
  response->set_code(code);
  response->set_content(content);
  return response;
}

}  // namespace

void CustomHttpResponse::SendResponse(
    base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) {
  std::string reason = "Custom";
  // The implementation of the BasicHttpResponse::reason() calls
  // net::GetHttpReasonPhrase, which requires status code to be a standard HTTP
  // status code and crashes otherwise. Hence we avoid calling it if a custom
  // HTTP code is used.
  // TODO(crbug.com/40209048): Make GetHttpReasonPhrase support custom codes
  // instead.
  if (base::ranges::lower_bound(kStandardHttpStatusCodes, code()) !=
      std::ranges::end(kStandardHttpStatusCodes)) {
    reason = BasicHttpResponse::reason();
  }
  delegate->SendHeadersContentAndFinish(code(), reason, BuildHeaders(),
                                        content());
}

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
             request, dm_protocol::kServiceTokenAuthHeaderPrefix, out) ||
         GetTokenFromAuthorization(request,
                                   dm_protocol::kOAuthTokenHeaderPrefix, out);
}

std::unique_ptr<HttpResponse> CreateHttpResponse(
    net::HttpStatusCode code,
    const em::DeviceManagementResponse& proto_content) {
  return CreateHttpResponse(code, proto_content.SerializeAsString(),
                            "application/x-protobuffer");
}

std::unique_ptr<HttpResponse> CreateHttpResponse(
    net::HttpStatusCode code,
    const std::string& text_content) {
  return CreateHttpResponse(code, text_content, "text/plain");
}

}  // namespace policy
