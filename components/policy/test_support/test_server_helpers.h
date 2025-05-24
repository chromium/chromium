// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_TEST_SUPPORT_TEST_SERVER_HELPERS_H_
#define COMPONENTS_POLICY_TEST_SUPPORT_TEST_SERVER_HELPERS_H_

#include <memory>
#include <string>

#include "components/policy/proto/device_management_backend.pb.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace net {
namespace test_server {
class HttpResponse;
struct HttpRequest;
}  // namespace test_server
}  // namespace net

namespace policy {

// HTTP Response that supports custom HTTP status codes.
class CustomHttpResponse : public net::test_server::BasicHttpResponse {
 public:
  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override;
};

// Returns the value associated with `key` in `url`'s query or empty string if
// `key` is not present.
std::string KeyValueFromUrl(GURL url, const std::string& key);

// Check server-side requirements, as defined in
// device_management_backend.proto.
bool MeetsServerSideRequirements(GURL url);

// Returns true if a token is specified in the request URL with prefix
// `token_header_prefix`, in which case the token is copied to `out`.

bool GetTokenFromAuthorization(const net::test_server::HttpRequest& request,
                               const std::string& token_header_prefix,
                               std::string* out);

// Returns true if an enrollment token is specified in the request URL, in which
// case the enrollment token is copied to `out`.
bool GetEnrollmentTokenFromRequest(const net::test_server::HttpRequest& request,
                                   std::string* out);

// Returns true if a device token is specified in the request URL, in which case
// the device token is copied to `out`.
bool GetDeviceTokenFromRequest(const net::test_server::HttpRequest& request,
                               std::string* out);

// Returns true if an auth toke is specified in the request URL with the
// oauth_token parameter or if it is set as GoogleLogin token from the
// Authorization header. The token is copied to `out` if available.
bool GetGoogleLoginFromRequest(const net::test_server::HttpRequest& request,
                               std::string* out);

// Returns a text/plain HttpResponse with a given `code` and `content`.
std::unique_ptr<net::test_server::HttpResponse> CreateHttpResponse(
    net::HttpStatusCode code,
    const std::string& content);

// Returns an application/x-protobuffer HttpResponse.
std::unique_ptr<net::test_server::HttpResponse> CreateHttpResponse(
    net::HttpStatusCode code,
    const enterprise_management::DeviceManagementResponse& content);

}  // namespace policy

#endif  // COMPONENTS_POLICY_TEST_SUPPORT_TEST_SERVER_HELPERS_H_
