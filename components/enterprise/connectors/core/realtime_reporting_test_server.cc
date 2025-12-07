// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/realtime_reporting_test_server.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"
#include "net/http/http_status_code.h"

namespace enterprise_connectors::test {

namespace {

using ::chrome::cros::reporting::proto::Event;
using ::chrome::cros::reporting::proto::EventResult;
using ::chrome::cros::reporting::proto::EventResult_Parse;
using ::chrome::cros::reporting::proto::LoginEvent;
using ::chrome::cros::reporting::proto::PasswordBreachEvent;
using ::chrome::cros::reporting::proto::SafeBrowsingInterstitialEvent;
using ::chrome::cros::reporting::proto::UploadEventsRequest;
using ::chrome::cros::reporting::proto::UrlFilteringInterstitialEvent;
using InterstitialThreatType = ::chrome::cros::reporting::proto::
    UrlFilteringInterstitialEvent_InterstitialThreatType;

constexpr char kRealtimeReportingUrl[] = "/v1/events";

void ParseLoginEvent(const base::Value::Dict* event_details_json,
                     LoginEvent* event) {
  if (const std::string* url = event_details_json->FindString("url")) {
    event->set_url(*url);
  }
  if (const std::string* login_user_name =
          event_details_json->FindString("loginUserName")) {
    event->set_login_user_name(*login_user_name);
  }
}

void ParsePasswordBreachEvent(const base::Value::Dict* event_details_json,
                              PasswordBreachEvent* event) {
  const base::Value::List* identities_json =
      event_details_json->FindList("identities");
  if (!identities_json) {
    return;
  }
  if (const std::string* trigger_type_name =
          event_details_json->FindString("trigger")) {
    PasswordBreachEvent::TriggerType trigger_type;
    if (PasswordBreachEvent::TriggerType_Parse(*trigger_type_name,
                                               &trigger_type)) {
      event->set_trigger(std::move(trigger_type));
    }
  }
  for (const base::Value& identity_json : *identities_json) {
    const base::Value::Dict* identity_json_dict = identity_json.GetIfDict();
    if (!identity_json_dict) {
      continue;
    }
    PasswordBreachEvent::Identity* identity = event->add_identities();
    if (const std::string* url = identity_json_dict->FindString("url")) {
      identity->set_url(*url);
    }
    if (const std::string* username =
            identity_json_dict->FindString("username")) {
      identity->set_username(*username);
    }
  }
}

void ParseInterstitialEvent(const base::Value::Dict* event_details_json,
                            SafeBrowsingInterstitialEvent* event) {
  if (const std::string* url = event_details_json->FindString("url")) {
    event->set_url(*url);
  }
  event->set_clicked_through(
      event_details_json->FindBool("clickedThrough").value_or(false));
  if (const std::string* event_result_name =
          event_details_json->FindString("eventResult")) {
    EventResult event_result;
    if (EventResult_Parse(*event_result_name, &event_result)) {
      event->set_event_result(std::move(event_result));
    }
  }
  if (const std::string* reason_name =
          event_details_json->FindString("reason")) {
    SafeBrowsingInterstitialEvent::InterstitialReason reason;
    if (SafeBrowsingInterstitialEvent::InterstitialReason_Parse(*reason_name,
                                                                &reason)) {
      event->set_reason(std::move(reason));
    }
  }
}

void ParseUrlFilteringInterstitialEvent(
    const base::Value::Dict* event_details_json,
    UrlFilteringInterstitialEvent* event) {
  if (const std::string* url = event_details_json->FindString("url")) {
    event->set_url(*url);
  }
  event->set_clicked_through(
      event_details_json->FindBool("clickedThrough").value_or(false));
  if (const std::string* event_result_name =
          event_details_json->FindString("eventResult")) {
    EventResult event_result;
    if (EventResult_Parse(*event_result_name, &event_result)) {
      event->set_event_result(std::move(event_result));
    }
  }
  if (const std::string* threat_type_name =
          event_details_json->FindString("threatType")) {
    UrlFilteringInterstitialEvent::InterstitialThreatType threat_type;
    if (UrlFilteringInterstitialEvent::InterstitialThreatType_Parse(
            *threat_type_name, &threat_type)) {
      event->set_threat_type(std::move(threat_type));
    }
  }
}

std::optional<Event> ParseEvent(const base::Value::Dict* event_json) {
  if (!event_json) {
    return std::nullopt;
  }
  Event event;
  const base::Value::Dict* event_details_json;
  // TODO(crbug.com/412683254): Add branches for other event types.
  if ((event_details_json = event_json->FindDict("loginEvent"))) {
    ParseLoginEvent(event_details_json, event.mutable_login_event());
  } else if ((event_details_json =
                  event_json->FindDict("passwordBreachEvent"))) {
    ParsePasswordBreachEvent(event_details_json,
                             event.mutable_password_breach_event());
  } else if ((event_details_json = event_json->FindDict("interstitialEvent"))) {
    ParseInterstitialEvent(event_details_json,
                           event.mutable_interstitial_event());
  } else if ((event_details_json =
                  event_json->FindDict("urlFilteringInterstitialEvent"))) {
    ParseUrlFilteringInterstitialEvent(
        event_details_json, event.mutable_url_filtering_interstitial_event());
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
  std::optional<base::Value::Dict> request_json = base::JSONReader::ReadDict(
      request.content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
