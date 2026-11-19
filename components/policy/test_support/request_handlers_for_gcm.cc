// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handlers_for_gcm.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace policy {

// RequestHandlerForGcmCheckin
RequestHandlerForGcmCheckin::RequestHandlerForGcmCheckin(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForGcmCheckin::~RequestHandlerForGcmCheckin() = default;

std::string RequestHandlerForGcmCheckin::RequestType() {
  return kGcmCheckinRequestType;
}

std::unique_ptr<net::test_server::HttpResponse>
RequestHandlerForGcmCheckin::HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_POST) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST, "Invalid request method.");
  }

  checkin_proto::AndroidCheckinRequest checkin_request;
  if (!checkin_request.ParseFromString(request.content)) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                              "Failed to parse AndroidCheckinRequest.");
  }

  checkin_proto::AndroidCheckinResponse checkin_response;
  checkin_response.set_stats_ok(true);
  checkin_response.set_time_msec(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  checkin_response.set_android_id(
      checkin_request.id() != 0 ? checkin_request.id() : 12345ULL);
  checkin_response.set_security_token(
      checkin_request.has_security_token() &&
              checkin_request.security_token() != 0
          ? checkin_request.security_token()
          : 67890ULL);

  return CreateHttpResponse(net::HTTP_OK, checkin_response);
}

// RequestHandlerForGcmRegister
RequestHandlerForGcmRegister::RequestHandlerForGcmRegister(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForGcmRegister::~RequestHandlerForGcmRegister() = default;

std::string RequestHandlerForGcmRegister::RequestType() {
  return kGcmRegisterRequestType;
}

std::unique_ptr<net::test_server::HttpResponse>
RequestHandlerForGcmRegister::HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_POST) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST, "Invalid request method.");
  }

  // The AidLogin header value is "<android_id>:<security_token>".
  std::string android_id;
  if (GetTokenFromAuthorization(request, "AidLogin ", &android_id)) {
    android_id = android_id.substr(0, android_id.find(':'));
  }

  std::string app;
  base::StringPairs pairs;
  base::SplitStringIntoKeyValuePairs(request.content, '=', '&', &pairs);
  for (const auto& [key, value] : pairs) {
    if (key == "device" && android_id.empty()) {
      android_id = value;
    } else if (key == "app" || key == "sender") {
      app = value;
    }
  }

  if (android_id.empty()) {
    android_id = "12345";
  }

  std::string token =
      app.empty() ? base::StrCat({"fake_fcm_token_", android_id})
                  : base::StrCat({"fake_fcm_token_", android_id, "_", app});

  return CreateHttpResponse(net::HTTP_OK, base::StrCat({"token=", token}));
}

}  // namespace policy
