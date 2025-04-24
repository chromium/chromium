// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/realtime_reporting_test_server.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "net/http/http_status_code.h"

namespace enterprise_connectors::test {

namespace {

using ::chrome::cros::reporting::proto::Event;
using ::chrome::cros::reporting::proto::LoginEvent;
using ::chrome::cros::reporting::proto::UploadEventsRequest;

constexpr char kRealtimeReportingUrl[] = "/v1/events";

std::optional<Event> ParseEvent(const base::Value::Dict* event_json) {
  if (!event_json) {
    return std::nullopt;
  }
  Event event;
  const base::Value::Dict* event_details_json;
  // TODO(crbug.com/412683254): Add branches for other event types.
  if ((event_details_json = event_json->FindDict("loginEvent"))) {
    LoginEvent* login_event = event.mutable_login_event();
    const std::string* url = event_details_json->FindString("url");
    if (url) {
      login_event->set_url(*url);
    }
    const std::string* login_user_name =
        event_details_json->FindString("loginUserName");
    if (login_user_name) {
      login_event->set_login_user_name(*login_user_name);
    }
  } else {
    return std::nullopt;
  }
  return event;
}

std::optional<UploadEventsRequest> ParseUploadEventsRequest(
    const net::test_server::HttpRequest& request) {
  UploadEventsRequest request_message;
  if (request_message.ParseFromString(request.content)) {
    return request_message;
  }

  // Fall back to parsing the legacy JSON event format. For simplicity, ignore
  // fields that aren't asserted by any test.
  std::optional<base::Value::Dict> request_json =
      base::JSONReader::ReadDict(request.content);
  if (!request_json) {
    return std::nullopt;
  }

  ::chrome::cros::reporting::proto::Device* device =
      request_message.mutable_device();
  const std::string* os_platform =
      request_json->FindStringByDottedPath("device.osPlatform");
  if (os_platform) {
    device->set_os_platform(*os_platform);
  }

  const base::Value::List* events_json = request_json->FindList("events");
  if (events_json) {
    for (const base::Value& event_json : *events_json) {
      std::optional<Event> event = ParseEvent(event_json.GetIfDict());
      if (event) {
        *request_message.add_events() = std::move(*event);
      }
    }
  }

  return request_message;
}

}  // namespace

RealtimeReportingTestServer::RealtimeReportingTestServer()
    : http_server_(net::test_server::EmbeddedTestServer::TYPE_HTTP) {
  http_server_.RegisterDefaultHandler(base::BindRepeating(
      &RealtimeReportingTestServer::HandleRequest, base::Unretained(this)));
}

RealtimeReportingTestServer::~RealtimeReportingTestServer() = default;

GURL RealtimeReportingTestServer::GetServiceURL() const {
  return http_server_.GetURL(kRealtimeReportingUrl);
}

bool RealtimeReportingTestServer::Start() {
  return !!(http_server_handle_ = http_server_.StartAndReturnHandle());
}

std::vector<UploadEventsRequest>
RealtimeReportingTestServer::GetUploadedReports() {
  base::AutoLock guard(reports_lock_);
  std::vector<UploadEventsRequest> reports(reports_);
  return reports;
}

std::unique_ptr<net::test_server::HttpResponse>
RealtimeReportingTestServer::HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (!request.relative_url.starts_with(kRealtimeReportingUrl)) {
    return nullptr;
  }

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  std::optional<UploadEventsRequest> request_message =
      ParseUploadEventsRequest(request);
  if (!request_message) {
    response->set_code(net::HttpStatusCode::HTTP_BAD_REQUEST);
    return response;
  }

  base::AutoLock guard(reports_lock_);
  reports_.push_back(std::move(*request_message));
  // The client doesn't look at the response payload, so omit the
  // `UploadEventsResponse` payload for simplicity.
  return response;
}

}  // namespace enterprise_connectors::test
