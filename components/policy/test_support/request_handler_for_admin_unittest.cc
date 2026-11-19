// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_admin.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/report_storage.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kDeviceId[] = "fake_device_id";
constexpr char kComputerName[] = "fake_computer_name";
constexpr char kOsUser[] = "fake_os_user";
constexpr char kSerialNumber[] = "fake_serial_number";
constexpr char kBrowserVersion[] = "123.0.6312.86";
constexpr char kProfileName[] = "fake_profile_name";
constexpr char kDeviceToken[] = "fake_device_token";
constexpr char kMachineName[] = "fake_machine_name";
constexpr char kUsername[] = "user@example.com";

// FakeDMServerTest covers the policy endpoints with the real delegate.
class FakeAdminDelegate : public RequestHandlerForAdmin::Delegate {
 public:
  std::string GetPoliciesJson() const override { return "{}"; }
  bool UpdatePoliciesJson(const std::string&) override { return true; }
  std::optional<bool> TriggerPolicyInvalidation() override {
    return invalidation_result_;
  }

  void set_invalidation_result(std::optional<bool> result) {
    invalidation_result_ = result;
  }

 private:
  std::optional<bool> invalidation_result_ = false;
};

}  // namespace

class RequestHandlerForAdminTest : public EmbeddedPolicyTestServerTestBase {
 protected:
  void SetUp() override {
    EmbeddedPolicyTestServerTestBase::SetUp();
    test_server()->RegisterHandler(
        std::make_unique<RequestHandlerForAdmin>(test_server(), delegate_));
  }

  // Sends a request to `path` and returns the response code and body.
  std::pair<int, std::string> SendRequest(
      const std::string& method,
      const std::string& path,
      const std::string& body = "",
      const std::string& content_type = "application/json",
      const std::string& origin = "") {
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->method = method;
    resource_request->url = test_server()->GetServiceURL().Resolve(path);
    if (!origin.empty()) {
      resource_request->headers.SetHeader("Origin", origin);
    }
    auto url_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);
    url_loader->SetAllowHttpErrorResults(true);
    if (method == net::HttpRequestHeaders::kPostMethod) {
      url_loader->AttachStringForUpload(body, content_type);
    }
    auto url_loader_factory =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
    base::RunLoop run_loop;
    std::string response_body;
    url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(),
        base::BindLambdaForTesting([&](std::optional<std::string> response) {
          response_body = response.value_or("");
          run_loop.Quit();
        }));
    run_loop.Run();
    return {url_loader->ResponseInfo()->headers->response_code(),
            response_body};
  }

  std::pair<int, std::string> Get(const std::string& path) {
    return SendRequest(net::HttpRequestHeaders::kGetMethod, path);
  }

  std::pair<int, std::string> Post(const std::string& path,
                                   const std::string& body) {
    return SendRequest(net::HttpRequestHeaders::kPostMethod, path, body);
  }

  FakeAdminDelegate delegate_;
};

TEST_F(RequestHandlerForAdminTest, HandleAdminApiReportsBrowser) {
  em::ChromeDesktopReportRequest desktop_report;
  desktop_report.set_computer_name(kComputerName);
  desktop_report.set_os_user_name(kOsUser);
  desktop_report.set_serial_number(kSerialNumber);
  desktop_report.mutable_browser_report()->set_browser_version(kBrowserVersion);
  report_storage()->AddBrowserReport(kDeviceId, std::move(desktop_report));

  auto [status, body] = Get("/admin/api/reports/browser");

  EXPECT_EQ(status, net::HTTP_OK);
  base::DictValue root = base::test::ParseJsonDict(body);
  const base::ListValue* list = root.FindList("reports");
  ASSERT_TRUE(list);
  ASSERT_EQ(list->size(), 1u);
  const base::DictValue& dict = (*list)[0].GetDict();
  EXPECT_EQ(*dict.FindString("device_id"), kDeviceId);
  EXPECT_NE(*dict.FindDouble("timestamp_ms"), 0.0);
  EXPECT_EQ(*dict.FindString("computer_name"), kComputerName);
  EXPECT_EQ(*dict.FindString("os_user"), kOsUser);
  EXPECT_EQ(*dict.FindString("serial_number"), kSerialNumber);
  EXPECT_EQ(*dict.FindString("browser_version"), kBrowserVersion);
}

TEST_F(RequestHandlerForAdminTest, HandleAdminApiReportsProfile) {
  // Without a stable profile ID, the ID falls back to the profile path.
  for (const char* profile_id : {"stable-profile-id-1", ""}) {
    em::ChromeProfileReportRequest profile_report;
    em::ChromeUserProfileInfo* profile_info =
        profile_report.mutable_browser_report()
            ->add_chrome_user_profile_infos();
    profile_info->set_name(kProfileName);
    profile_info->set_id("/path/to/profile");
    if (*profile_id) {
      profile_info->set_profile_id(profile_id);
    }
    report_storage()->AddProfileReport(kDeviceId, std::move(profile_report));
  }

  auto [status, body] = Get("/admin/api/reports/profile");

  EXPECT_EQ(status, net::HTTP_OK);
  base::DictValue root = base::test::ParseJsonDict(body);
  const base::ListValue* list = root.FindList("reports");
  ASSERT_TRUE(list);
  ASSERT_EQ(list->size(), 2u);
  const base::DictValue& dict = (*list)[0].GetDict();
  EXPECT_EQ(*dict.FindString("device_id"), kDeviceId);
  EXPECT_NE(*dict.FindDouble("timestamp_ms"), 0.0);
  EXPECT_EQ(*dict.FindString("name"), kProfileName);
  EXPECT_EQ(*dict.FindString("id"), "stable-profile-id-1");
  EXPECT_EQ(*dict.FindString("profile_path"), "/path/to/profile");
  EXPECT_EQ(*(*list)[1].GetDict().FindString("id"), "/path/to/profile");
}

TEST_F(RequestHandlerForAdminTest, HandleAdminApiClients) {
  ClientStorage::ClientInfo client_info;
  client_info.device_id = kDeviceId;
  client_info.device_token = kDeviceToken;
  client_info.machine_name = kMachineName;
  client_info.username = kUsername;
  client_storage()->RegisterClient(std::move(client_info));

  auto [status, body] = Get("/admin/api/clients");

  EXPECT_EQ(status, net::HTTP_OK);
  base::DictValue root = base::test::ParseJsonDict(body);
  const base::ListValue* list = root.FindList("clients");
  ASSERT_TRUE(list);
  ASSERT_EQ(list->size(), 1u);
  const base::DictValue& dict = (*list)[0].GetDict();
  EXPECT_EQ(*dict.FindString("device_id"), kDeviceId);
  EXPECT_EQ(*dict.FindString("device_token"), kDeviceToken);
  EXPECT_EQ(*dict.FindString("machine_name"), kMachineName);
  EXPECT_EQ(*dict.FindString("username"), kUsername);
}

TEST_F(RequestHandlerForAdminTest, HandleAdminApiCommandsScopeRouting) {
  const struct {
    const char* scope;
    int command_type;
    const char* content_type;
    em::RemoteCommand::Type expected_type;
    RemoteCommandScope expected_scope;
    RemoteCommandScope other_scope;
  } kTestCases[] = {
      {"browser", 12, "application/json",
       em::RemoteCommand::BROWSER_CLEAR_BROWSING_DATA,
       RemoteCommandScope::kBrowser, RemoteCommandScope::kProfile},
      {"profile", 18, "application/json; charset=utf-8",
       em::RemoteCommand::BROWSER_EXTENSION_UPDATE_CHECK,
       RemoteCommandScope::kProfile, RemoteCommandScope::kBrowser},
      {"profile", -1, "application/json", em::RemoteCommand::COMMAND_ECHO_TEST,
       RemoteCommandScope::kProfile, RemoteCommandScope::kBrowser},
      {"browser", 1, "application/json", em::RemoteCommand::DEVICE_SCREENSHOT,
       RemoteCommandScope::kBrowser, RemoteCommandScope::kProfile},
  };

  for (const auto& tc : kTestCases) {
    auto [status, body] = SendRequest(
        net::HttpRequestHeaders::kPostMethod, "/admin/api/commands",
        absl::StrFormat(
            R"({"scope": "%s", "command_type": %d, "payload": "{}"})", tc.scope,
            tc.command_type),
        tc.content_type);

    EXPECT_EQ(status, net::HTTP_OK);
    base::DictValue dict = base::test::ParseJsonDict(body);
    EXPECT_TRUE(dict.FindBool("success").value_or(false));
    std::optional<double> command_id = dict.FindDouble("command_id");
    ASSERT_TRUE(command_id.has_value());

    EXPECT_TRUE(remote_commands_state()
                    ->ExtractPendingRemoteCommands(tc.other_scope)
                    .empty());
    std::vector<em::RemoteCommand> commands =
        remote_commands_state()->ExtractPendingRemoteCommands(
            tc.expected_scope);
    ASSERT_EQ(commands.size(), 1u);
    EXPECT_EQ(commands[0].command_id(), static_cast<int64_t>(*command_id));
    EXPECT_EQ(commands[0].type(), tc.expected_type);
  }
}

TEST_F(RequestHandlerForAdminTest, HandleAdminApiCommandsErrors) {
  const struct {
    const char* body;
    const char* expected_error;
  } kTestCases[] = {
      {"not-a-json", "Invalid JSON"},
      {R"({"scope": "unknown", "command_type": 12})", "Invalid scope"},
      {R"({"scope": "browser"})", "Invalid command_type"},
      {R"({"scope": "browser", "command_type": -2})", "Invalid command_type"},
      {R"({"scope": "profile", "command_type": 9999})", "Invalid command_type"},
  };

  for (const auto& tc : kTestCases) {
    auto [status, body] = Post("/admin/api/commands", tc.body);
    EXPECT_EQ(status, net::HTTP_BAD_REQUEST);
    EXPECT_EQ(*base::test::ParseJsonDict(body).FindString("error"),
              tc.expected_error);
  }
}

TEST_F(RequestHandlerForAdminTest,
       HandleAdminApiPostChecksOriginAndContentType) {
  constexpr char kValidBody[] =
      R"({"scope": "browser", "command_type": 12, "payload": "{}"})";

  // Foreign origin must be rejected with HTTP 403.
  auto [status, body] =
      SendRequest(net::HttpRequestHeaders::kPostMethod, "/admin/api/commands",
                  kValidBody, "application/json", "https://evil.example");
  EXPECT_EQ(status, net::HTTP_FORBIDDEN);
  EXPECT_EQ(*base::test::ParseJsonDict(body).FindString("error"),
            "Forbidden Origin");
  EXPECT_TRUE(remote_commands_state()->ExtractPendingRemoteCommands().empty());

  // Localhost origin must be accepted.
  std::tie(status, body) =
      SendRequest(net::HttpRequestHeaders::kPostMethod, "/admin/api/commands",
                  kValidBody, "application/json", "http://localhost:8080");
  EXPECT_EQ(status, net::HTTP_OK);
  EXPECT_FALSE(remote_commands_state()->ExtractPendingRemoteCommands().empty());

  // All the POST endpoints check the origin and the content type.
  for (const char* path :
       {"/admin/api/policies", "/admin/api/policies/invalidate"}) {
    EXPECT_EQ(SendRequest(net::HttpRequestHeaders::kPostMethod, path, "{}",
                          "application/json", "https://evil.example")
                  .first,
              net::HTTP_FORBIDDEN);
    EXPECT_EQ(SendRequest(net::HttpRequestHeaders::kPostMethod, path, "{}",
                          "text/plain")
                  .first,
              net::HTTP_UNSUPPORTED_MEDIA_TYPE);
  }
}

TEST_F(RequestHandlerForAdminTest, HandleAdminApiCommandsGetStatusAndResult) {
  auto get_commands = [&] {
    auto [status, body] = Get("/admin/api/commands");
    EXPECT_EQ(status, net::HTTP_OK);
    return std::move(*base::test::ParseJsonDict(body).FindList("commands"));
  };
  EXPECT_TRUE(get_commands().empty());

  auto [status, body] =
      Post("/admin/api/commands", R"({"scope": "browser", "command_type": 12, )"
                                  R"("payload": "{\"param\": 1}"})");
  EXPECT_EQ(status, net::HTTP_OK);
  int64_t command_id = static_cast<int64_t>(
      *base::test::ParseJsonDict(body).FindDouble("command_id"));

  base::ListValue commands = get_commands();
  ASSERT_EQ(commands.size(), 1u);
  const base::DictValue& pending = commands[0].GetDict();
  EXPECT_EQ(*pending.FindDouble("command_id"), command_id);
  EXPECT_EQ(*pending.FindString("type"), "BROWSER_CLEAR_BROWSING_DATA");
  EXPECT_EQ(*pending.FindString("payload"), R"({"param": 1})");
  EXPECT_EQ(*pending.FindString("status"), "PENDING");
  EXPECT_FALSE(pending.FindString("result"));

  remote_commands_state()->ExtractPendingRemoteCommands();
  commands = get_commands();
  ASSERT_EQ(commands.size(), 1u);
  EXPECT_EQ(*commands[0].GetDict().FindString("status"), "FETCHED");
  EXPECT_FALSE(commands[0].GetDict().FindString("result"));

  em::RemoteCommandResult result;
  result.set_command_id(command_id);
  result.set_result(em::RemoteCommandResult::RESULT_SUCCESS);
  result.set_payload(R"({"status": "ok"})");
  result.set_timestamp(987654321);
  remote_commands_state()->AddRemoteCommandResult(result);

  commands = get_commands();
  ASSERT_EQ(commands.size(), 1u);
  const base::DictValue& done = commands[0].GetDict();
  EXPECT_EQ(*done.FindString("status"), "RESULT_AVAILABLE");
  EXPECT_EQ(*done.FindString("result"), "RESULT_SUCCESS");
  EXPECT_EQ(*done.FindString("result_payload"), R"({"status": "ok"})");
  EXPECT_EQ(*done.FindDouble("timestamp_ms"), 987654321);
}

TEST_F(RequestHandlerForAdminTest, HandleAdminApiReportsEvents) {
  ::chrome::cros::reporting::proto::UploadEventsRequest request;
  request.add_events()->mutable_login_event()->set_url(
      "https://example.com/login");
  request.add_events()
      ->mutable_password_breach_event()
      ->add_identities()
      ->set_username("alice");
  report_storage()->AddRealtimeEvents(request);

  auto [status, body] = Get("/admin/api/reports/events");
  EXPECT_EQ(status, net::HTTP_OK);
  base::DictValue dict = base::test::ParseJsonDict(body);
  const base::ListValue* events = dict.FindList("events");
  ASSERT_TRUE(events);
  ASSERT_EQ(events->size(), 2u);
  EXPECT_EQ(*(*events)[0].GetDict().FindString("event_type"), "loginEvent");
  EXPECT_EQ(*(*events)[1].GetDict().FindString("event_type"),
            "passwordBreachEvent");
}

TEST_F(RequestHandlerForAdminTest, HandleAdminApiPoliciesInvalidate) {
  delegate_.set_invalidation_result(true);
  auto [status, body] = Post("/admin/api/policies/invalidate", "");
  EXPECT_EQ(status, net::HTTP_OK);
  base::DictValue dict = base::test::ParseJsonDict(body);
  EXPECT_EQ(dict.FindBool("success"), true);
  EXPECT_EQ(dict.FindBool("sent"), true);

  delegate_.set_invalidation_result(false);
  std::tie(status, body) = Post("/admin/api/policies/invalidate", "");
  EXPECT_EQ(status, net::HTTP_OK);
  dict = base::test::ParseJsonDict(body);
  EXPECT_EQ(dict.FindBool("success"), true);
  EXPECT_EQ(dict.FindBool("sent"), false);

  delegate_.set_invalidation_result(std::nullopt);
  std::tie(status, body) = Post("/admin/api/policies/invalidate", "");
  EXPECT_EQ(status, net::HTTP_BAD_REQUEST);
  EXPECT_EQ(*base::test::ParseJsonDict(body).FindString("error"),
            "Invalid policy file");
}

}  // namespace policy
