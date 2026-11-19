// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_admin.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/enterprise/connectors/core/reporting_event_mappings.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/remote_commands_state.h"
#include "components/policy/test_support/report_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace policy {

namespace em = enterprise_management;

namespace {

using net::test_server::HttpResponse;

// The dashboard is listed in `data` of the BUILD.gn target, which keeps its
// path relative to the source root.
std::unique_ptr<HttpResponse> CreateDashboardResponse() {
  base::FilePath root_dir;
  std::string html;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_dir) ||
      !base::ReadFileToString(
          root_dir.AppendASCII(
              "components/policy/test_support/admin_dashboard.html"),
          &html)) {
    return CreateHttpResponse(net::HTTP_NOT_FOUND, "Dashboard not found.");
  }
  return CreateHttpResponse(net::HTTP_OK, html, "text/html; charset=utf-8");
}

std::string_view EnumNameOrUnknown(std::string_view name) {
  return name.empty() ? "UNKNOWN" : name;
}

std::unique_ptr<HttpResponse> CreateJsonResponse(net::HttpStatusCode code,
                                                 base::ValueView value) {
  return CreateHttpResponse(code, base::WriteJson(value).value_or(""),
                            "application/json");
}

std::unique_ptr<HttpResponse> CreateJsonListResponse(std::string_view key,
                                                     base::ListValue list) {
  return CreateJsonResponse(net::HTTP_OK,
                            base::DictValue().Set(key, std::move(list)));
}

std::unique_ptr<HttpResponse> CreateJsonError(net::HttpStatusCode code,
                                              std::string_view error) {
  return CreateJsonResponse(code, base::DictValue().Set("error", error));
}

// Rejects cross-origin POST requests. They must have a JSON content type and,
// if present, a localhost origin.
std::unique_ptr<HttpResponse> CheckPostRequest(
    const net::test_server::HttpRequest& request) {
  if (auto it = request.headers.find("Origin"); it != request.headers.end()) {
    GURL origin(it->second);
    if (!origin.is_valid() || !net::IsLocalhost(origin)) {
      return CreateJsonError(net::HTTP_FORBIDDEN, "Forbidden Origin");
    }
  }
  auto it = request.headers.find("Content-Type");
  if (it == request.headers.end() ||
      !base::StartsWith(base::ToLowerASCII(it->second), "application/json")) {
    return CreateJsonError(net::HTTP_UNSUPPORTED_MEDIA_TYPE,
                           "Unsupported Media Type");
  }
  return nullptr;
}

template <typename EntryType, typename FieldExtractor>
base::ListValue BuildReportList(const std::vector<EntryType>& reports,
                                FieldExtractor extract_fields) {
  base::ListValue list;
  for (const auto& entry : reports) {
    base::DictValue dict;
    dict.Set("device_id", entry.device_id);
    dict.Set(
        "timestamp_ms",
        static_cast<double>(entry.timestamp.InMillisecondsSinceUnixEpoch()));
    extract_fields(entry, dict);
    list.Append(std::move(dict));
  }
  return list;
}

base::ListValue BuildBrowserReportsList(
    const std::vector<ReportStorage::BrowserReportEntry>& reports) {
  return BuildReportList(reports, [](const auto& entry, base::DictValue& dict) {
    const auto& r = entry.report;
    dict.Set("computer_name", r.has_computer_name()  ? r.computer_name()
                              : r.has_machine_name() ? r.machine_name()
                                                     : "");
    dict.Set("os_user", r.has_os_user_name() ? r.os_user_name()
                        : r.has_os_user()    ? r.os_user()
                                             : "");
    dict.Set("serial_number", r.has_serial_number() ? r.serial_number() : "");
    dict.Set("browser_version",
             r.has_browser_report() && r.browser_report().has_browser_version()
                 ? r.browser_report().browser_version()
                 : "");
  });
}

base::ListValue BuildClientsList(
    const std::vector<ClientStorage::ClientInfo>& clients) {
  base::ListValue list;
  for (const auto& client : clients) {
    base::DictValue dict;
    dict.Set("device_id", client.device_id);
    dict.Set("device_token", client.device_token);
    dict.Set("machine_name", client.machine_name);
    dict.Set("username", client.username.value_or(""));
    list.Append(std::move(dict));
  }
  return list;
}

base::ListValue BuildProfileReportsList(
    const std::vector<ReportStorage::ProfileReportEntry>& reports) {
  return BuildReportList(reports, [](const auto& entry, base::DictValue& dict) {
    std::string name;
    std::string id;
    std::string profile_path;
    if (entry.report.has_browser_report() &&
        !entry.report.browser_report().chrome_user_profile_infos().empty()) {
      const auto& info =
          entry.report.browser_report().chrome_user_profile_infos(0);
      if (info.has_name()) {
        name = info.name();
      }
      if (info.has_id()) {
        profile_path = info.id();
      }
      id = info.has_profile_id() ? info.profile_id() : info.id();
    }
    dict.Set("name", name);
    dict.Set("id", id);
    dict.Set("profile_path", profile_path);
  });
}

std::string_view IssuedCommandStatusToString(
    IssuedRemoteCommand::Status status) {
  switch (status) {
    case IssuedRemoteCommand::Status::kPending:
      return "PENDING";
    case IssuedRemoteCommand::Status::kFetched:
      return "FETCHED";
    case IssuedRemoteCommand::Status::kResultAvailable:
      return "RESULT_AVAILABLE";
  }
}

base::DictValue BuildCommandDict(const IssuedRemoteCommand& issued) {
  const em::RemoteCommand& command = issued.command;
  base::DictValue dict;
  dict.Set("command_id", static_cast<double>(command.command_id()));
  dict.Set("type",
           EnumNameOrUnknown(em::RemoteCommand::Type_Name(command.type())));
  dict.Set("payload", command.payload());
  dict.Set("status", IssuedCommandStatusToString(issued.status));
  if (issued.result) {
    dict.Set("result",
             EnumNameOrUnknown(em::RemoteCommandResult::ResultType_Name(
                 issued.result->result())));
    dict.Set("result_payload", issued.result->payload());
    dict.Set("timestamp_ms", static_cast<double>(issued.result->timestamp()));
  }
  return dict;
}

}  // namespace

RequestHandlerForAdmin::RequestHandlerForAdmin(
    EmbeddedPolicyTestServer* test_server,
    Delegate& delegate)
    : EmbeddedPolicyTestServer::RequestHandler(test_server),
      delegate_(delegate) {}

RequestHandlerForAdmin::~RequestHandlerForAdmin() = default;

std::string RequestHandlerForAdmin::RequestType() {
  return kAdminRequestType;
}

std::unique_ptr<HttpResponse> RequestHandlerForAdmin::HandleRequest(
    const net::test_server::HttpRequest& request) {
  std::string path = request.GetURL().GetPath();

  if (request.method == net::test_server::METHOD_POST) {
    if (auto error = CheckPostRequest(request)) {
      return error;
    }
  }

  if (path == "/admin" || path == "/admin/") {
    return CreateDashboardResponse();
  }

  if (path == "/admin/api/reports/browser") {
    return CreateJsonListResponse(
        "reports",
        BuildBrowserReportsList(report_storage()->GetBrowserReports()));
  }

  if (path == "/admin/api/reports/profile") {
    return CreateJsonListResponse(
        "reports",
        BuildProfileReportsList(report_storage()->GetProfileReports()));
  }

  if (path == "/admin/api/reports/events") {
    base::ListValue events_list;
    for (const auto& event : report_storage()->GetRealtimeEvents()) {
      events_list.Append(base::DictValue().Set(
          "event_type",
          enterprise_connectors::GetEventName(event.event_case())));
    }
    return CreateJsonListResponse("events", std::move(events_list));
  }

  if (path == "/admin/api/clients") {
    return CreateJsonListResponse(
        "clients", BuildClientsList(client_storage()->GetAllClients()));
  }

  if (path == "/admin/api/commands") {
    if (request.method == net::test_server::METHOD_GET) {
      base::ListValue commands_list;
      for (const IssuedRemoteCommand& issued :
           remote_commands_state()->GetIssuedCommands()) {
        commands_list.Append(BuildCommandDict(issued));
      }
      return CreateJsonListResponse("commands", std::move(commands_list));
    }

    if (request.method != net::test_server::METHOD_POST) {
      return CreateHttpResponse(net::HTTP_METHOD_NOT_ALLOWED, "");
    }

    std::optional<base::DictValue> dict =
        base::JSONReader::ReadDict(request.content, base::JSON_PARSE_RFC);
    if (!dict) {
      return CreateJsonError(net::HTTP_BAD_REQUEST, "Invalid JSON");
    }

    const std::string* scope = dict->FindString("scope");
    if (!scope || (*scope != "browser" && *scope != "profile")) {
      return CreateJsonError(net::HTTP_BAD_REQUEST, "Invalid scope");
    }

    std::optional<int> command_type = dict->FindInt("command_type");
    if (!command_type.has_value() ||
        !em::RemoteCommand::Type_IsValid(*command_type)) {
      return CreateJsonError(net::HTTP_BAD_REQUEST, "Invalid command_type");
    }

    enterprise_management::RemoteCommand command;
    command.set_type(
        static_cast<enterprise_management::RemoteCommand::Type>(*command_type));
    if (const std::string* payload = dict->FindString("payload")) {
      command.set_payload(*payload);
    }
    int64_t command_id = remote_commands_state()->AddPendingRemoteCommand(
        command, *scope == "profile" ? RemoteCommandScope::kProfile
                                     : RemoteCommandScope::kBrowser);

    return CreateJsonResponse(
        net::HTTP_OK, base::DictValue()
                          .Set("success", true)
                          .Set("command_id", static_cast<double>(command_id)));
  }

  if (path == "/admin/api/policies") {
    if (request.method == net::test_server::METHOD_GET) {
      return CreateHttpResponse(net::HTTP_OK, delegate_->GetPoliciesJson(),
                                "application/json");
    }

    if (request.method == net::test_server::METHOD_POST) {
      if (!delegate_->UpdatePoliciesJson(request.content)) {
        return CreateJsonError(net::HTTP_BAD_REQUEST, "Invalid policies");
      }
      return CreateJsonResponse(net::HTTP_OK,
                                base::DictValue().Set("success", true));
    }

    return CreateHttpResponse(net::HTTP_METHOD_NOT_ALLOWED, "");
  }

  if (path == "/admin/api/policies/invalidate") {
    if (request.method != net::test_server::METHOD_POST) {
      return CreateHttpResponse(net::HTTP_METHOD_NOT_ALLOWED, "");
    }

    std::optional<bool> sent = delegate_->TriggerPolicyInvalidation();
    if (!sent.has_value()) {
      return CreateJsonError(net::HTTP_BAD_REQUEST, "Invalid policy file");
    }
    return CreateJsonResponse(
        net::HTTP_OK,
        base::DictValue().Set("success", true).Set("sent", *sent));
  }

  return CreateHttpResponse(net::HTTP_NOT_FOUND, "");
}

}  // namespace policy
