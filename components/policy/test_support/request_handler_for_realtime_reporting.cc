// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_realtime_reporting.h"

#include <string>

#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/policy/test_support/report_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace policy {

RequestHandlerForRealtimeReporting::RequestHandlerForRealtimeReporting(
    EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForRealtimeReporting::~RequestHandlerForRealtimeReporting() =
    default;

std::string RequestHandlerForRealtimeReporting::RequestType() {
  return kRealtimeReportingRequestType;
}

std::unique_ptr<net::test_server::HttpResponse>
RequestHandlerForRealtimeReporting::HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.method != net::test_server::METHOD_POST) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST, "Invalid request method.");
  }

  std::string dm_token;
  if (!GetDeviceTokenFromRequest(request, &dm_token) || dm_token.empty()) {
    return CreateHttpResponse(net::HTTP_UNAUTHORIZED, "Invalid device token.");
  }

  ::chrome::cros::reporting::proto::UploadEventsRequest upload_request;
  if (!upload_request.ParseFromString(request.content)) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                              "Failed to parse UploadEventsRequest.");
  }

  report_storage()->AddRealtimeEvents(upload_request);

  ::chrome::cros::reporting::proto::UploadEventsResponse response;
  for (const auto& event : upload_request.events()) {
    if (!event.event_id().empty()) {
      response.add_uploaded_event_ids(event.event_id());
    }
  }
  return CreateHttpResponse(net::HTTP_OK, response);
}

}  // namespace policy
