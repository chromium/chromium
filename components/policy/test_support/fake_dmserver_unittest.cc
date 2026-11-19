// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/fake_dmserver.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/invalidation/invalidation_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/fake_fcm_server.h"
#include "components/policy/test_support/fcm_test_util.h"
#include "components/policy/test_support/policy_invalidator.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/remote_commands_state.h"
#include "components/policy/test_support/report_storage.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fakedms {

namespace {

constexpr std::string_view kRawExtensionPolicyPayload =
    R"({
      "VisibleStringPolicy": {
        "Value": "notsecret"
      },
      "SensitiveStringPolicy": {
        "Value": "secret"
      },
      "VisibleDictPolicy": {
        "Value": {
          "some_bool": true,
          "some_string": "notsecret"
        }
      },
      "SensitiveDictPolicy": {
        "Value": {
          "some_bool": true,
          "some_string": "secret"
        }
      }
    })";
constexpr std::string_view kPolicyBlobForExternalPolicy =
    R"(
    {
      "managed_users" : [ "*" ],
      "policy_user" : "tast-user@managedchrome.com",
      "external_policies": [
        {
          "entity_id": "ibdnofdagboejmpijdiknapcihkomkki",
          "policy_type": "google/chrome/extension",
          "value": "ewogICAgICAiVmlzaWJsZVN0cmluZ1BvbGljeSI6IHsKICAgICAgICAiVm)"
    R"(FsdWUiOiAibm90c2VjcmV0IgogICAgICB9LAogICAgICAiU2Vuc2l0aXZlU3RyaW5nUG9sa)"
    R"(WN5IjogewogICAgICAgICJWYWx1ZSI6ICJzZWNyZXQiCiAgICAgIH0sCiAgICAgICJWaXNp)"
    R"(YmxlRGljdFBvbGljeSI6IHsKICAgICAgICAiVmFsdWUiOiB7CiAgICAgICAgICAic29tZV9)"
    R"(ib29sIjogdHJ1ZSwKICAgICAgICAgICJzb21lX3N0cmluZyI6ICJub3RzZWNyZXQiCiAgIC)"
    R"(AgICAgfQogICAgICB9LAogICAgICAiU2Vuc2l0aXZlRGljdFBvbGljeSI6IHsKICAgICAgI)"
    R"(CAiVmFsdWUiOiB7CiAgICAgICAgICAic29tZV9ib29sIjogdHJ1ZSwKICAgICAgICAgICJz)"
    R"(b21lX3N0cmluZyI6ICJzZWNyZXQiCiAgICAgICAgfQogICAgICB9CiAgICB9"
        }
      ]
    }
  )";
constexpr std::string_view kSHA256HashForExtensionPolicyPayload(
    "\x1e\x95\xf3\xeb\x42\xcc\x72\x2c\x83\xdb\x2d\x1c\xb1\xca\xfa\x2b\x78\x1e"
    "\x4b\x91\x2b\x73\x1a\x5c\x85\x72\xa8\xf2\x87\x4a\xbc\x44",
    32);

class Response {
 public:
  Response(int status, std::string mime_type, std::string raw_body)
      : status(status), mime_type_(std::move(mime_type)) {
    if (mime_type_ == "application/x-protobuffer") {
      body = em::DeviceManagementResponse();
      proto_parse_success_ =
          std::get<em::DeviceManagementResponse>(body).ParseFromString(
              raw_body);
    } else {
      body = std::move(raw_body);
    }
  }

  void AssertValidProto() {
    ASSERT_EQ(mime_type_, "application/x-protobuffer");
    ASSERT_TRUE(proto_parse_success_) << "Proto parsing failed.";
    ASSERT_TRUE(is_proto());
  }

  void AssertText() {
    ASSERT_EQ(mime_type_, "text/plain");
    ASSERT_TRUE(is_text());
  }

  const em::DeviceManagementResponse& proto() const {
    CHECK(is_proto());
    return std::get<em::DeviceManagementResponse>(body);
  }

  const std::string& text() const {
    CHECK(is_text());
    return std::get<std::string>(body);
  }

  int status;
  std::variant<std::string, em::DeviceManagementResponse> body;

 private:
  bool is_proto() const {
    return std::holds_alternative<em::DeviceManagementResponse>(body);
  }
  bool is_text() const { return std::holds_alternative<std::string>(body); }

  bool proto_parse_success_;
  std::string mime_type_;
};

std::string GetStanzaType(const mcs_proto::DataMessageStanza& stanza) {
  for (int i = 0; i < stanza.app_data_size(); ++i) {
    if (stanza.app_data(i).key() == "type") {
      return stanza.app_data(i).value();
    }
  }
  return "";
}

// Expects that `stanza` is an invalidation from the `project_number` project.
void ExpectFcmProject(const mcs_proto::DataMessageStanza& stanza,
                      int64_t project_number) {
  const std::string from = base::NumberToString(project_number);
  EXPECT_EQ(stanza.from(), from);
  EXPECT_EQ(stanza.category(), "com.google.chrome.fcm.invalidations-" + from);
}

// Reads the browser and user policy invalidations pushed to `client`.
void ExpectPolicyInvalidationStanzas(net::StreamSocket& client) {
  std::vector<std::string> types;
  for (int i = 0; i < 2; ++i) {
    mcs_proto::DataMessageStanza stanza;
    ASSERT_TRUE(ReadFcmPacket(client, gcm::kDataMessageStanzaTag, &stanza));
    ExpectFcmProject(stanza,
                     invalidation::kNonCriticalInvalidationsProjectNumber);
    types.push_back(GetStanzaType(stanza));
  }
  EXPECT_THAT(types,
              testing::UnorderedElementsAre(kBrowserPolicyInvalidationType,
                                            kUserPolicyInvalidationType));
}

// Adds a browser remote command as a marker, and expects that the next stanza
// that `client` receives is the marker. Thus no other stanza came before it.
void ExpectNextStanzaIsMarker(FakeDMServer& fake_dmserver,
                              net::StreamSocket& client) {
  em::RemoteCommand command;
  command.set_type(em::RemoteCommand::COMMAND_ECHO_TEST);
  fake_dmserver.remote_commands_state()->AddPendingRemoteCommand(
      command, policy::RemoteCommandScope::kBrowser);
  mcs_proto::DataMessageStanza stanza;
  ASSERT_TRUE(ReadFcmPacket(client, gcm::kDataMessageStanzaTag, &stanza));
  EXPECT_EQ(GetStanzaType(stanza), kBrowserRemoteCommandInvalidationType);
}

}  // namespace

class FakeDMServerTest : public testing::Test {
 public:
  FakeDMServerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    policy_blob_path_ = temp_dir_.GetPath().Append(
        base::FilePath(FILE_PATH_LITERAL("policy.json")));
    ASSERT_FALSE(PathExists(policy_blob_path_));
    client_state_path_ = temp_dir_.GetPath().Append(
        base::FilePath(FILE_PATH_LITERAL("state.json")));
    ASSERT_FALSE(PathExists(client_state_path_));
  }

  // Returns a server whose debounce timer runs on `debounce_runner_`.
  FakeDMServer CreateFakeDMServer(
      base::OnceClosure shutdown_cb = base::DoNothing()) {
    return FakeDMServer(
        policy_blob_path_.MaybeAsASCII(), client_state_path_.MaybeAsASCII(),
        /*grpc_unix_socket_path=*/"", debounce_runner_, std::move(shutdown_cb));
  }

  // Waits until the file watcher has seen a change of the policy file.
  void WaitForPolicyFileChange() {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return debounce_runner_->HasPendingTask(); }));
  }

  // Returns the number of files that match `pattern` in the data directory.
  size_t CountDataFiles(const base::FilePath::StringType& pattern) {
    base::FileEnumerator enumerator(client_state_path_.DirName(),
                                    /*recursive=*/false,
                                    base::FileEnumerator::FILES, pattern);
    size_t count = 0;
    for (base::FilePath file = enumerator.Next(); !file.empty();
         file = enumerator.Next()) {
      ++count;
    }
    return count;
  }

  Response SendRequest(
      const GURL& server_url,
      const std::string& request_path,
      std::optional<em::DeviceManagementRequest> request_proto = std::nullopt) {
    return request_proto ? SendRequest(server_url, request_path,
                                       request_proto->SerializeAsString(),
                                       "application/x-protobuffer")
                         : SendRequest(server_url, request_path, "", "");
  }

  // Sends `body` with `content_type`, or no body if `content_type` is empty.
  Response SendRequest(const GURL& server_url,
                       const std::string& request_path,
                       const std::string& body,
                       const std::string& content_type) {
    std::string request_url =
        base::StringPrintf("http://%s:%s%s", server_url.GetHost().c_str(),
                           server_url.GetPort().c_str(), request_path.c_str());
    std::unique_ptr<network::ResourceRequest> resource_request =
        std::make_unique<network::ResourceRequest>();
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
    resource_request->url = GURL(request_url);
    resource_request->headers.SetHeader(
        "Authorization", "GoogleDMToken token=fake_device_token");

    std::unique_ptr<network::SimpleURLLoader> url_loader =
        network::SimpleURLLoader::Create(std::move(resource_request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    url_loader->SetAllowHttpErrorResults(true);
    if (!content_type.empty()) {
      url_loader->AttachStringForUpload(body, content_type);
    }
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();

    base::test::TestFuture<std::optional<std::string>> test_future;
    url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(), test_future.GetCallback());
    std::optional<std::string> response_body = test_future.Take();
    if (response_body) {
      LOG(INFO) << "Response body: " << *response_body;
    }
    LOG(INFO) << "Response headers: "
              << url_loader->ResponseInfo()->headers->raw_headers();
    int response_code = url_loader->ResponseInfo()->headers->response_code();
    if (response_body) {
      std::string mime_type;
      CHECK(url_loader->ResponseInfo()->headers->GetMimeType(&mime_type));
      return Response(response_code, mime_type, std::move(*response_body));
    }
    return Response(response_code, "", "");
  }

 protected:
  base::FilePath policy_blob_path_, client_state_path_;
  scoped_refptr<base::TestMockTimeTaskRunner> debounce_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();

 private:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(FakeDMServerTest, HandleExitRequestSucceeds) {
  base::MockOnceCallback<void()> callback;
  FakeDMServer fake_dmserver = CreateFakeDMServer(callback.Get());
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  EXPECT_CALL(callback, Run());
  Response response = SendRequest(fake_dmserver.GetServiceURL(), "/test/exit");
  EXPECT_EQ(response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(response.AssertText());
  EXPECT_EQ(response.text(), "Policy Server exited.");
}

TEST_F(FakeDMServerTest, HandlePingRequestSucceeds) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  Response response = SendRequest(fake_dmserver.GetServiceURL(), "/test/ping");
  EXPECT_EQ(response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(response.AssertText());
  EXPECT_EQ(response.text(), "Pong.");
}

TEST_F(FakeDMServerTest, HandleRegisterRequestSucceeds) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "policy_user" : "tast-user@managedchrome.com"
    }
  )"));

  Response response = SendRequest(fake_dmserver.GetServiceURL(),
                                  "/?apptype=Chrome&deviceid=fake_device_id&"
                                  "devicetype=2&oauth_token=fake_policy_token&"
                                  "request=register");
  EXPECT_EQ(response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(response.AssertValidProto());
  EXPECT_TRUE(response.proto().has_register_response());
  EXPECT_EQ(response.proto().register_response().machine_name(),
            " - fake_device_id");

  // Check if the data of the registered client is correct and written to the
  // client state file.
  std::vector<policy::ClientStorage::ClientInfo> clients =
      fake_dmserver.client_storage()->GetAllClients();
  ASSERT_EQ(clients.size(), 1u);
  EXPECT_EQ(clients[0].device_id, "fake_device_id");
  EXPECT_FALSE(clients[0].device_token.empty());
  EXPECT_FALSE(clients[0].machine_name.empty());
  EXPECT_EQ(clients[0].username.value(), "tast-user@managedchrome.com");
  ASSERT_EQ(clients[0].allowed_policy_types.size(), 1u);
  EXPECT_EQ(*clients[0].allowed_policy_types.begin(),
            policy::dm_protocol::GetChromeUserPolicyType());
  EXPECT_TRUE(clients[0].state_keys.empty());

  JSONFileValueDeserializer deserializer(client_state_path_);
  int error_code = 0;
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  ASSERT_TRUE(value);
  const base::DictValue* state_dict = value->GetIfDict();
  ASSERT_TRUE(state_dict);
  ASSERT_EQ(state_dict->size(), 1u);
  const base::DictValue* client_dict = state_dict->FindDict("fake_device_id");
  ASSERT_TRUE(client_dict);
  const std::string* device_id = client_dict->FindString("device_id");
  ASSERT_TRUE(device_id);
  EXPECT_EQ(*device_id, "fake_device_id");
  const std::string* device_token = client_dict->FindString("device_token");
  ASSERT_TRUE(device_token);
  EXPECT_FALSE(device_token->empty());
  const std::string* machine_name = client_dict->FindString("machine_name");
  ASSERT_TRUE(machine_name);
  EXPECT_FALSE(machine_name->empty());
  const std::string* username = client_dict->FindString("username");
  ASSERT_TRUE(username);
  EXPECT_EQ(*username, "tast-user@managedchrome.com");

  const base::ListValue* allowed_policy_types =
      client_dict->FindList("allowed_policy_types");
  ASSERT_TRUE(allowed_policy_types);
  ASSERT_EQ(allowed_policy_types->size(), 1u);
  EXPECT_EQ((*allowed_policy_types)[0].GetString(),
            policy::dm_protocol::GetChromeUserPolicyType());

  const base::ListValue* state_keys = client_dict->FindList("state_keys");
  ASSERT_TRUE(state_keys);
  EXPECT_TRUE(state_keys->empty());
}

TEST_F(FakeDMServerTest, ReadClientStateFileWithWrongJSONDataFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(client_state_path_, "wrong data"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, ReadClientStateFileWithNonDictFileFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(client_state_path_, R"([ "1", "2" ])"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, GetClientFromValueWithNonDictValueFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(client_state_path_,
                              R"({ "fake_device_id" : "not dict" })"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, GetClientFromValueWithOnlyDeviceIDFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(
      client_state_path_,
      R"({ "fake_device_id" : { "device_id" : "fake_device_id" } })"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, GetClientFromValueWithNonStringDeviceIDFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(client_state_path_,
                              R"({ "fake_device_id" : { "device_id" : 7 } })"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, GetClientFromValueWithoutStateKeyListFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(client_state_path_, R"(
    {
      "fake_device_id" : {
        "device_id" : "fake_device_id",
        "device_token" : "fake_device_token",
        "machine_name" : "fake_machine_name",
        "username" : "tast-user@managedchrome.com",
        "allowed_policy_types" : [ "google/chromeos/user" ]
      }
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, GetClientFromValueWithNonStringStateKeysFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(client_state_path_, R"(
    {
      "fake_device_id" : {
        "device_id" : "fake_device_id",
        "device_token" : "fake_device_token",
        "machine_name" : "fake_machine_name",
        "username" : "tast-user@managedchrome.com",
        "state_keys" : [ 7 ],
        "allowed_policy_types" : [ "google/chromeos/user" ]
      }
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, GetClientFromValueNoUsernameSucceeds) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(client_state_path_, R"(
    {
      "fake_device_id" : {
        "device_id" : "fake_device_id",
        "device_token" : "fake_device_token",
        "machine_name" : "fake_machine_name",
        "state_keys" : [ "fake_state_key" ],
        "allowed_policy_types" : [ "google/chromeos/user" ]
      }
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_OK);
}

TEST_F(FakeDMServerTest, GetClientFromValueWithNonStringPolicyTypesFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(client_state_path_, R"(
    {
      "fake_device_id" : {
        "device_id" : "fake_device_id",
        "device_token" : "fake_device_token",
        "machine_name" : "fake_machine_name",
        "username" : "tast-user@managedchrome.com",
        "state_keys" : [ "fake_state_key" ],
        "allowed_policy_types" : [ 7 ]
      }
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, HandlePolicyRequestSucceeds) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(
      policy_blob_path_,
      base::StringPrintf(
          R"(
    {
      "managed_users" : [ "*" ],
      "policy_user" : "tast-user@managedchrome.com",
      "policies" : [
        {
          "policy_type" : "%s", "value" : "uhMCEAE="
        }, {
          "policy_type" : "google/chromeos/device",
          "value" : "qgFSCikSJWRlZmF1bHRNZ3NTZXRCeVRhc3RAbWFuYWdlZGNocm9tZS5jb)"
          R"(20YABIlZGVmYXVsdE1nc1NldEJ5VGFzdEBtYW5hZ2VkY2hyb21lLmNvbQ=="
        }, {
          "entity_id" : "accountid@managedchrome.com",
          "policy_type" : "google/chromeos/publicaccount",
          "value" : "ojCsARKpAXsiaGFzaCI6IjdhMDUyYzVlNGYyM2MxNTk2NjgxNDhkZjJhM)"
          R"(2MyMDJiZWQ0ZDY1NzQ5Y2FiNWVjZDBmYTdkYjIxMWMxMmEzYjgiLCJ1cmwiOiJodHRwcz)"
          R"(ovL3N0b3JhZ2UuZ29vZ2xlYXBpcy5jb20vY2hyb21pdW1vcy10ZXN0LWFzc2V0cy1wdWJ)"
          R"(saWMvZW50ZXJwcmlzZS9wcmludGVycy5qc29uIn0="
        }
      ],
      "current_key_index": 1,
      "robot_api_auth_code": "code",
      "directory_api_id": "id",
      "device_affiliation_ids" : [
        "device_id"
      ],
      "user_affiliation_ids" : [
        "user_id"
      ],
      "allow_set_device_attributes" : false,
      "initial_enrollment_state": {
        "TEST_serial": {
          "initial_enrollment_mode": 2,
          "management_domain": "test-domain.com"
        }
      }
    }
  )",
          policy::dm_protocol::GetChromeUserPolicyType())));
  ASSERT_TRUE(base::WriteFile(
      client_state_path_,
      base::StringPrintf(R"(
    {
      "fake_device_id" : {
        "device_id" : "fake_device_id",
        "device_token" : "fake_device_token",
        "machine_name" : "fake_machine_name",
        "username" : "tast-user@managedchrome.com",
        "state_keys" : [ "fake_state_key" ],
        "allowed_policy_types" : [ "google/chrome/extension",
        "%s", "google/chromeos/device", "google/chromeos/publicaccount" ]
      }
    }
  )",
                         policy::dm_protocol::GetChromeUserPolicyType())));

  {
    em::DeviceManagementRequest request_proto;
    request_proto.mutable_policy_request()->add_requests()->set_policy_type(
        policy::dm_protocol::GetChromeUserPolicyType());
    Response response =
        SendRequest(fake_dmserver.GetServiceURL(),
                    "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                    "oauth_token=fake_policy_token&request=policy",
                    std::move(request_proto));
    EXPECT_EQ(response.status, net::HTTP_OK);
    ASSERT_NO_FATAL_FAILURE(response.AssertValidProto());
    EXPECT_EQ(response.proto().policy_response().responses_size(), 1);
    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(
        response.proto().policy_response().responses(0).policy_data()));
    EXPECT_EQ(policy_data.policy_type(),
              policy::dm_protocol::GetChromeUserPolicyType());
    EXPECT_EQ(base::Base64Encode(policy_data.policy_value()), "uhMCEAE=");
  }

  {
    em::DeviceManagementRequest request_proto;
    request_proto.mutable_policy_request()->add_requests()->set_policy_type(
        "google/chromeos/device");
    Response response =
        SendRequest(fake_dmserver.GetServiceURL(),
                    "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                    "oauth_token=fake_policy_token&request=policy",
                    std::move(request_proto));
    EXPECT_EQ(response.status, net::HTTP_OK);
    ASSERT_NO_FATAL_FAILURE(response.AssertValidProto());
    EXPECT_EQ(response.proto().policy_response().responses_size(), 1);
    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(
        response.proto().policy_response().responses(0).policy_data()));
    EXPECT_EQ(
        base::Base64Encode(policy_data.policy_value()),
        "qgFSCikSJWRlZmF1bHRNZ3NTZXRCeVRhc3RAbWFuYWdlZGNocm9tZS5jb20YABIlZG"
        "VmYXVsdE1nc1NldEJ5VGFzdEBtYW5hZ2VkY2hyb21lLmNvbQ==");
  }

  std::string publicaccount_policy_payload =
      fake_dmserver.policy_storage()->GetPolicyPayload(
          "google/chromeos/publicaccount", "accountid@managedchrome.com");
  std::string public_policy_output =
      base::Base64Encode(publicaccount_policy_payload);
  EXPECT_EQ(public_policy_output,
            "ojCsARKpAXsiaGFzaCI6IjdhMDUyYzVlNGYyM2MxNTk2NjgxNDhkZjJhM2MyMDJiZW"
            "Q0ZDY1NzQ5Y2FiNWVjZDBmYTdkYjIxMWMxMmEzYjgiLCJ1cmwiOiJodHRwczovL3N0"
            "b3JhZ2UuZ29vZ2xlYXBpcy5jb20vY2hyb21pdW1vcy10ZXN0LWFzc2V0cy1wdWJsaW"
            "MvZW50ZXJwcmlzZS9wcmludGVycy5qc29uIn0=");

  int current_key_index = fake_dmserver.policy_storage()
                              ->signature_provider()
                              ->current_key_version();
  EXPECT_EQ(current_key_index, 1);

  std::string robot_api_auth_code =
      fake_dmserver.policy_storage()->robot_api_auth_code();
  EXPECT_EQ(robot_api_auth_code, "code");

  std::string directory_api_id =
      fake_dmserver.policy_storage()->directory_api_id();
  EXPECT_EQ(directory_api_id, "id");

  bool allow_set_device_attributes =
      fake_dmserver.policy_storage()->allow_set_device_attributes();
  EXPECT_FALSE(allow_set_device_attributes);

  std::vector<std::string> device_affiliation_ids =
      fake_dmserver.policy_storage()->device_affiliation_ids();
  ASSERT_EQ(device_affiliation_ids.size(), 1u);
  EXPECT_EQ(device_affiliation_ids[0], "device_id");

  std::vector<std::string> user_affiliation_ids =
      fake_dmserver.policy_storage()->user_affiliation_ids();
  ASSERT_EQ(user_affiliation_ids.size(), 1u);
  EXPECT_EQ(user_affiliation_ids[0], "user_id");

  const policy::PolicyStorage::InitialEnrollmentState*
      initial_enrollment_state =
          fake_dmserver.policy_storage()->GetInitialEnrollmentState(
              "TEST_serial");
  ASSERT_TRUE(initial_enrollment_state);
  EXPECT_EQ(initial_enrollment_state->management_domain, "test-domain.com");
  EXPECT_EQ(
      initial_enrollment_state->initial_enrollment_mode,
      static_cast<enterprise_management::DeviceInitialEnrollmentStateResponse::
                      InitialEnrollmentMode>(2));
}

TEST_F(FakeDMServerTest, HandlePolicyRequestWithCustomErrorSucceeds) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_,
                              R"(
    {
      "managed_users" : [ "*" ],
      "policy_user" : "tast-user@managedchrome.com",
      "request_errors": { "policy": 500 },
      "policies" : [
        {
          "policy_type" : "google/chromeos/user", "value" : "uhMCEAE="
        }
      ]
    }
  )"));
  ASSERT_TRUE(base::WriteFile(client_state_path_, R"(
    {
      "fake_device_id" : {
        "device_id" : "fake_device_id",
        "device_token" : "fake_device_token",
        "machine_name" : "fake_machine_name",
        "username" : "tast-user@managedchrome.com",
        "state_keys" : [ "fake_state_key" ],
        "allowed_policy_types" : [ "google/chrome/extension",
        "google/chromeos/user" ]
      }
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, HandleExternalPolicyRequestSucceeds) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, kPolicyBlobForExternalPolicy));
  ASSERT_TRUE(base::WriteFile(client_state_path_, R"(
    {
      "fake_device_id" : {
        "device_id" : "fake_device_id",
        "device_token" : "fake_device_token",
        "machine_name" : "fake_machine_name",
        "username" : "tast-user@managedchrome.com",
        "state_keys" : [ "fake_state_key" ],
        "allowed_policy_types" : [ "google/chrome/extension",
        "google/chromeos/user" ]
      }
    }
  )"));

  em::DeviceManagementRequest request_proto;
  auto* policy_request = request_proto.mutable_policy_request()->add_requests();
  policy_request->set_policy_type("google/chrome/extension");
  policy_request->set_settings_entity_id("ibdnofdagboejmpijdiknapcihkomkki");
  Response response =
      SendRequest(fake_dmserver.GetServiceURL(),
                  "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                  "oauth_token=fake_policy_token&request=policy",
                  std::move(request_proto));
  EXPECT_EQ(response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(response.AssertValidProto());
  EXPECT_EQ(response.proto().policy_response().responses_size(), 1);

  em::PolicyData policy_data;
  ASSERT_TRUE(policy_data.ParseFromString(
      response.proto().policy_response().responses(0).policy_data()));
  EXPECT_EQ(policy_data.policy_type(), "google/chrome/extension");

  em::ExternalPolicyData external_policy_data;
  ASSERT_TRUE(external_policy_data.ParseFromString(policy_data.policy_value()));
  EXPECT_TRUE(external_policy_data.has_download_url());
  EXPECT_EQ(external_policy_data.secure_hash(),
            kSHA256HashForExtensionPolicyPayload);
  ASSERT_TRUE(external_policy_data.has_download_url());

  GURL download_url(external_policy_data.download_url());
  ASSERT_TRUE(download_url.is_valid());
  Response second_response =
      SendRequest(fake_dmserver.GetServiceURL(), download_url.PathForRequest());
  EXPECT_EQ(second_response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(second_response.AssertText());
  EXPECT_EQ(second_response.text(), kRawExtensionPolicyPayload);
}

TEST_F(FakeDMServerTest, ReadPolicyBlobFileWithWrongJSONDataFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, "wrong data"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, ReadPolicyBlobFileWithNonDictFileFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"([ "1", "2" ])"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, ReadPolicyBlobFileWithNonDictPoliciesFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "policies" : [ "1", "2" ]
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, ReadPolicyBlobFileWithNonDictExternalPoliciesFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "external_policies" : [ "1", "2" ]
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, ReadPolicyBlobFileWithNonIntRequestErrorFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "policy_user" : "tast-user@managedchrome.com",
      "request_errors": { "policy": "non int value" },
      "policies" : [
        {
          "policy_type" : "google/chromeos/user", "value" : "uhMCEAE="
        }
      ]
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest,
       ReadPolicyBlobFileWithNonBoolAllowSetDeviceAttributesFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "policy_user" : "tast-user@managedchrome.com",
      "allow_set_device_attributes": { "key": "non int value" },
      "policies" : [
        {
          "policy_type" : "google/chromeos/user", "value" : "uhMCEAE="
        }
      ]
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, ReadPolicyBlobFileWithNonStringManagementDomainFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "policy_user" : "tast-user@managedchrome.com",
      "initial_enrollment_state":
      {
        "management_domain": 3,
        "initial_enrollment_mode": 1
      },
      "policies" : [
        {
          "policy_type" : "google/chromeos/user", "value" : "uhMCEAE="
        }
      ]
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest,
       ReadPolicyBlobFileWithNonIntInitialEnrollmentModeFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "policy_user" : "tast-user@managedchrome.com",
      "initial_enrollment_state":
      {
        "management_domain": "domain",
        "initial_enrollment_mode": "non int value"
      },
      "policies" : [
        {
          "policy_type" : "google/chromeos/user", "value" : "uhMCEAE="
        }
      ]
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, ReadPolicyBlobFileWithNonIntCurrentKeyIndexFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "policy_user" : "tast-user@managedchrome.com",
      "current_key_index": "non int value",
      "policies" : [
        {
          "policy_type" : "google/chromeos/user", "value" : "uhMCEAE="
        }
      ]
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, SetPolicyPayloadWithoutValueOrTypeFieldFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "policies" : [
        { "wrong type" : "google/chromeos/user", "wrong value" : "uhMCEAE=" }
      ]
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, SetPolicyPayloadWithNonBase64ValueFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "policies" : [
        { "policy_type" : "google/chromeos/user", "value" : "!@#$%^&*" }
      ]
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, SetExternalPolicyPayloadWithoutValueOrTypeFieldFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "external_policies" : [
        {
          "wrong type" : "google/chrome/extension",
          "wrong id" : "random id",
          "wrong value" : "!@#$%^&*"
        }
      ]
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, SetExternalPolicyPayloadWithNonBase64ValueFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "managed_users" : [ "*" ],
      "external_policies" : [
        {
          "policy_type" : "google/chrome/extension",
          "entity_id" : "random_id",
          "value" : "!@#$%^&*"
        }
      ]
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, HandleExtensionInstallPolicyRequestSucceeds) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "policy_user": "foo@example.com",
      "managed_users": [
        "*"
      ],
      "policies": [
        {
          "policy_type": "google/extension-install-cloud-policy/chrome/machine",
          "entity_id": "abcdefghijklmnopqrstuvwxyzabcdef@67.67.67",
          "value": "CjAKIGFiY2RlZmdoaWprbG1ub3BxcnN0dXZ3eHl6YWJjZGVmEgg2Ny42Ny42NxgCIAE="
        }
      ]
    }
  )"));
  ASSERT_TRUE(base::WriteFile(client_state_path_, R"(
    {
      "fake_device_id" : {
        "device_id" : "fake_device_id",
        "device_token" : "fake_device_token",
        "machine_name" : "fake_machine_name",
        "state_keys": [],
        "allowed_policy_types" : [
          "google/extension-install-cloud-policy/chrome/machine" ]
      }
    }
  )"));

  {
    // Fetch an existing extension via settings_entity_id.
    em::DeviceManagementRequest request_proto;
    auto* policy_request =
        request_proto.mutable_policy_request()->add_requests();
    policy_request->set_policy_type(
        "google/extension-install-cloud-policy/chrome/machine");
    policy_request->set_settings_entity_id(
        "abcdefghijklmnopqrstuvwxyzabcdef@67.67.67");

    Response response =
        SendRequest(fake_dmserver.GetServiceURL(),
                    "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                    "oauth_token=fake_policy_token&request=policy",
                    std::move(request_proto));
    EXPECT_EQ(response.status, net::HTTP_OK);
    ASSERT_NO_FATAL_FAILURE(response.AssertValidProto());
    EXPECT_EQ(response.proto().policy_response().responses_size(), 1);

    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(
        response.proto().policy_response().responses(0).policy_data()));
    EXPECT_EQ(policy_data.policy_type(),
              "google/extension-install-cloud-policy/chrome/machine");

    em::ExtensionInstallPolicies extension_install_policies;
    ASSERT_TRUE(
        extension_install_policies.ParseFromString(policy_data.policy_value()));
    EXPECT_EQ(extension_install_policies.policies_size(), 1);

    em::ExtensionInstallPolicy extension_install_policy =
        extension_install_policies.policies(0);
    EXPECT_EQ(extension_install_policy.extension_id(),
              "abcdefghijklmnopqrstuvwxyzabcdef");
    EXPECT_EQ(extension_install_policy.extension_version(), "67.67.67");
    EXPECT_EQ(extension_install_policy.action(),
              em::ExtensionInstallPolicy::ACTION_BLOCK);
    EXPECT_EQ(extension_install_policy.reasons_size(), 1);
    EXPECT_EQ(extension_install_policy.reasons(0),
              em::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY);
  }

  {
    // Fetch an existing extension via extension_ids_and_version.
    em::DeviceManagementRequest request_proto;
    auto* policy_request =
        request_proto.mutable_policy_request()->add_requests();
    policy_request->set_policy_type(
        "google/extension-install-cloud-policy/chrome/machine");
    auto* extension_ids_and_version =
        policy_request->add_extension_ids_and_version();
    extension_ids_and_version->set_extension_id(
        "abcdefghijklmnopqrstuvwxyzabcdef");
    extension_ids_and_version->set_extension_version("67.67.67");

    Response response =
        SendRequest(fake_dmserver.GetServiceURL(),
                    "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                    "oauth_token=fake_policy_token&request=policy",
                    std::move(request_proto));
    EXPECT_EQ(response.status, net::HTTP_OK);
    ASSERT_NO_FATAL_FAILURE(response.AssertValidProto());
    EXPECT_EQ(response.proto().policy_response().responses_size(), 1);

    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(
        response.proto().policy_response().responses(0).policy_data()));
    EXPECT_EQ(policy_data.policy_type(),
              "google/extension-install-cloud-policy/chrome/machine");

    em::ExtensionInstallPolicies extension_install_policies;
    ASSERT_TRUE(
        extension_install_policies.ParseFromString(policy_data.policy_value()));
    EXPECT_EQ(extension_install_policies.policies_size(), 1);

    em::ExtensionInstallPolicy extension_install_policy =
        extension_install_policies.policies(0);
    EXPECT_EQ(extension_install_policy.extension_id(),
              "abcdefghijklmnopqrstuvwxyzabcdef");
    EXPECT_EQ(extension_install_policy.extension_version(), "67.67.67");
    EXPECT_EQ(extension_install_policy.action(),
              em::ExtensionInstallPolicy::ACTION_BLOCK);
    EXPECT_EQ(extension_install_policy.reasons_size(), 1);
    EXPECT_EQ(extension_install_policy.reasons(0),
              em::ExtensionInstallPolicy::REASON_BLOCKED_CATEGORY);
  }

  {
    // Try to fetch a non-existing extension, and one with a different version.
    // Request still succeeds, but with an empty ExtensionInstallPolicies
    // payload.
    em::DeviceManagementRequest request_proto;
    auto* policy_request =
        request_proto.mutable_policy_request()->add_requests();
    policy_request->set_policy_type(
        "google/extension-install-cloud-policy/chrome/machine");
    auto* extension_ids_and_version =
        policy_request->add_extension_ids_and_version();
    extension_ids_and_version->set_extension_id(
        "bcdefghijklmnopqrstuvwxyzabcdefg");
    extension_ids_and_version->set_extension_version("67.67.67");

    extension_ids_and_version = policy_request->add_extension_ids_and_version();
    extension_ids_and_version->set_extension_id(
        "abcdefghijklmnopqrstuvwxyzabcdef");
    extension_ids_and_version->set_extension_version("67.67.68");

    Response response =
        SendRequest(fake_dmserver.GetServiceURL(),
                    "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                    "oauth_token=fake_policy_token&request=policy",
                    std::move(request_proto));
    EXPECT_EQ(response.status, net::HTTP_OK);
    ASSERT_NO_FATAL_FAILURE(response.AssertValidProto());
    EXPECT_EQ(response.proto().policy_response().responses_size(), 1);

    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(
        response.proto().policy_response().responses(0).policy_data()));
    EXPECT_EQ(policy_data.policy_type(),
              "google/extension-install-cloud-policy/chrome/machine");

    em::ExtensionInstallPolicies extension_install_policies;
    ASSERT_TRUE(
        extension_install_policies.ParseFromString(policy_data.policy_value()));
    EXPECT_EQ(extension_install_policies.policies_size(), 0);
  }
}

TEST_F(FakeDMServerTest, HandlePolicyRequestWithJsonFormatSucceeds) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "policy_user" : "tast-user@managedchrome.com",
      "machine": {
        "AllowDinosaurEasterEgg": true,
        "ExtensionInstallForcelist": [
          "extension_id_1;https://example.com/update1",
          "extension_id_2"
        ]
      },
      "user": {
        "HomepageLocation": "http://example.com",
        "ManagedBookmarks": [
          {
            "name": "Google",
            "url": "google.com"
          }
        ]
      }
    }
  )"));
  ASSERT_TRUE(base::WriteFile(
      client_state_path_,
      base::StringPrintf(
          R"(
    {
      "fake_device_id" : {
        "device_id" : "fake_device_id",
        "device_token" : "fake_device_token",
        "machine_name" : "fake_machine_name",
        "username" : "tast-user@managedchrome.com",
        "state_keys" : [ "fake_state_key" ],
        "allowed_policy_types" : [
          "%s",
          "%s"
        ]
      }
    }
  )",
          policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
          policy::dm_protocol::GetChromeUserPolicyType())));

  em::DeviceManagementRequest request_proto;
  request_proto.mutable_policy_request()->add_requests()->set_policy_type(
      policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  request_proto.mutable_policy_request()->add_requests()->set_policy_type(
      policy::dm_protocol::GetChromeUserPolicyType());

  Response response =
      SendRequest(fake_dmserver.GetServiceURL(),
                  "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                  "oauth_token=fake_policy_token&request=policy",
                  std::move(request_proto));
  EXPECT_EQ(response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(response.AssertValidProto());
  EXPECT_EQ(response.proto().policy_response().responses_size(), 2);

  base::flat_map<std::string, em::PolicyFetchResponse> responses;
  for (const auto& r : response.proto().policy_response().responses()) {
    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(r.policy_data()));
    responses[policy_data.policy_type()] = r;
  }

  {
    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(
        responses[policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType]
            .policy_data()));
    em::CloudPolicySettings settings;
    ASSERT_TRUE(settings.ParseFromString(policy_data.policy_value()));
    EXPECT_TRUE(settings.has_allowdinosaureasteregg());
    EXPECT_TRUE(settings.allowdinosaureasteregg().value());
    EXPECT_TRUE(settings.has_extensioninstallforcelist());
    EXPECT_THAT(
        settings.extensioninstallforcelist().value().entries(),
        testing::ElementsAre("extension_id_1;https://example.com/update1",
                             "extension_id_2"));
  }

  {
    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(
        responses[policy::dm_protocol::GetChromeUserPolicyType()]
            .policy_data()));
    em::CloudPolicySettings settings;
    ASSERT_TRUE(settings.ParseFromString(policy_data.policy_value()));
    EXPECT_TRUE(settings.has_homepagelocation());
    EXPECT_EQ(settings.homepagelocation().value(), "http://example.com");
    EXPECT_TRUE(settings.has_managedbookmarks());
    EXPECT_EQ(settings.managedbookmarks().value(),
              R"([{"name":"Google","url":"google.com"}])");
    EXPECT_EQ(settings.managedbookmarks().policy_options().mode(),
              em::PolicyOptions::MANDATORY);
  }
}

TEST_F(FakeDMServerTest, HandlePolicyRequestWithRecommendedJsonFormatSucceeds) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "policy_user" : "tast-user@managedchrome.com",
      "machine": {
        "AllowDinosaurEasterEgg": true
      },
      "recommended_machine": {
        "ShowHomeButton": false,
        "RestoreOnStartup": 4
      },
      "user": {
        "ShowHomeButton": true
      },
      "recommended_user": {
        "BookmarkBarEnabled": true
      }
    }
  )"));
  ASSERT_TRUE(base::WriteFile(
      client_state_path_,
      base::StringPrintf(
          R"(
    {
      "fake_device_id" : {
        "device_id" : "fake_device_id",
        "device_token" : "fake_device_token",
        "machine_name" : "fake_machine_name",
        "username" : "tast-user@managedchrome.com",
        "state_keys" : [ "fake_state_key" ],
        "allowed_policy_types" : [
          "%s",
          "%s"
        ]
      }
    }
  )",
          policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
          policy::dm_protocol::GetChromeUserPolicyType())));

  em::DeviceManagementRequest request_proto;
  request_proto.mutable_policy_request()->add_requests()->set_policy_type(
      policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  request_proto.mutable_policy_request()->add_requests()->set_policy_type(
      policy::dm_protocol::GetChromeUserPolicyType());

  Response response =
      SendRequest(fake_dmserver.GetServiceURL(),
                  "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                  "oauth_token=fake_policy_token&request=policy",
                  std::move(request_proto));
  EXPECT_EQ(response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(response.AssertValidProto());
  EXPECT_EQ(response.proto().policy_response().responses_size(), 2);

  base::flat_map<std::string, em::PolicyFetchResponse> responses;
  for (const auto& r : response.proto().policy_response().responses()) {
    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(r.policy_data()));
    responses[policy_data.policy_type()] = r;
  }

  {
    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(
        responses[policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType]
            .policy_data()));
    em::CloudPolicySettings settings;
    ASSERT_TRUE(settings.ParseFromString(policy_data.policy_value()));

    EXPECT_TRUE(settings.has_allowdinosaureasteregg());
    EXPECT_TRUE(settings.allowdinosaureasteregg().value());
    EXPECT_EQ(settings.allowdinosaureasteregg().policy_options().mode(),
              em::PolicyOptions::MANDATORY);

    EXPECT_TRUE(settings.has_showhomebutton());
    EXPECT_FALSE(settings.showhomebutton().value());
    EXPECT_EQ(settings.showhomebutton().policy_options().mode(),
              em::PolicyOptions::RECOMMENDED);

    EXPECT_TRUE(settings.has_restoreonstartup());
    EXPECT_EQ(settings.restoreonstartup().value(), 4);
    EXPECT_EQ(settings.restoreonstartup().policy_options().mode(),
              em::PolicyOptions::RECOMMENDED);
  }

  {
    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(
        responses[policy::dm_protocol::GetChromeUserPolicyType()]
            .policy_data()));
    em::CloudPolicySettings settings;
    ASSERT_TRUE(settings.ParseFromString(policy_data.policy_value()));

    // Verify machine policies did not leak into user policies.
    EXPECT_FALSE(settings.has_allowdinosaureasteregg());
    EXPECT_FALSE(settings.has_restoreonstartup());

    EXPECT_TRUE(settings.has_showhomebutton());
    EXPECT_TRUE(settings.showhomebutton().value());
    EXPECT_EQ(settings.showhomebutton().policy_options().mode(),
              em::PolicyOptions::MANDATORY);

    EXPECT_TRUE(settings.has_bookmarkbarenabled());
    EXPECT_TRUE(settings.bookmarkbarenabled().value());
    EXPECT_EQ(settings.bookmarkbarenabled().policy_options().mode(),
              em::PolicyOptions::RECOMMENDED);
  }
}

TEST_F(FakeDMServerTest,
       HandleExtensionInstallPolicyRequestWithJsonFormatSucceeds) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "policy_user" : "tast-user@managedchrome.com",
      "machine-extension-install": {
        "abcdefghijklmnopabcdefghijklmnop@1.0.0": {
          "action": "block",
          "reasons": ["risk_score"]
        }
      }
    }
  )"));
  ASSERT_TRUE(base::WriteFile(
      client_state_path_,
      base::StringPrintf(
          R"(
    {
      "fake_device_id" : {
        "device_id" : "fake_device_id",
        "device_token" : "fake_device_token",
        "machine_name" : "fake_machine_name",
        "username" : "tast-user@managedchrome.com",
        "state_keys" : [ "fake_state_key" ],
        "allowed_policy_types" : [
          "%s"
        ]
      }
    }
  )",
          policy::dm_protocol::
              kChromeExtensionInstallMachineLevelCloudPolicyType)));

  em::DeviceManagementRequest request_proto;
  auto* policy_request = request_proto.mutable_policy_request()->add_requests();
  policy_request->set_policy_type(
      policy::dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType);
  policy_request->set_settings_entity_id(
      "abcdefghijklmnopabcdefghijklmnop@1.0.0");

  Response response =
      SendRequest(fake_dmserver.GetServiceURL(),
                  "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                  "oauth_token=fake_policy_token&request=policy",
                  std::move(request_proto));
  EXPECT_EQ(response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(response.AssertValidProto());
  EXPECT_EQ(response.proto().policy_response().responses_size(), 1);

  em::PolicyData policy_data;
  ASSERT_TRUE(policy_data.ParseFromString(
      response.proto().policy_response().responses(0).policy_data()));
  EXPECT_EQ(
      policy_data.policy_type(),
      policy::dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType);

  em::ExtensionInstallPolicies extension_install_policies;
  ASSERT_TRUE(
      extension_install_policies.ParseFromString(policy_data.policy_value()));
  EXPECT_EQ(extension_install_policies.policies_size(), 1);

  em::ExtensionInstallPolicy extension_install_policy =
      extension_install_policies.policies(0);
  EXPECT_EQ(extension_install_policy.extension_id(),
            "abcdefghijklmnopabcdefghijklmnop");
  EXPECT_EQ(extension_install_policy.extension_version(), "1.0.0");
  EXPECT_EQ(extension_install_policy.action(),
            em::ExtensionInstallPolicy::ACTION_BLOCK);
  EXPECT_EQ(extension_install_policy.reasons_size(), 1);
  EXPECT_EQ(extension_install_policy.reasons(0),
            em::ExtensionInstallPolicy::REASON_RISK_SCORE);
}

TEST_F(FakeDMServerTest,
       HandleExtensionCloudPolicyRequestWithJsonFormatSucceeds) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  static constexpr char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  const char* const kPolicyType =
      policy::dm_protocol::kChromeMachineLevelExtensionCloudPolicyType;
  ASSERT_TRUE(
      base::WriteFile(policy_blob_path_, base::StringPrintf(R"(
    {
      "policy_user": "tast-user@managedchrome.com",
      "machine-extensions": {
        "%s": {
          "ShorthandPolicy": 123,
          "ExplicitPolicy": {"Value": true, "Level": "Recommended"}
        }
      }
    }
  )",
                                                            kExtensionId)));
  ASSERT_TRUE(
      base::WriteFile(client_state_path_, base::StringPrintf(R"(
    {
      "fake_device_id": {
        "device_id": "fake_device_id",
        "device_token": "fake_device_token",
        "machine_name": "fake_machine_name",
        "username": "tast-user@managedchrome.com",
        "state_keys": [ "fake_state_key" ],
        "allowed_policy_types": [ "%s" ]
      }
    }
  )",
                                                             kPolicyType)));

  em::DeviceManagementRequest request_proto;
  auto* policy_request = request_proto.mutable_policy_request()->add_requests();
  policy_request->set_policy_type(kPolicyType);
  policy_request->set_settings_entity_id(kExtensionId);
  Response response =
      SendRequest(fake_dmserver.GetServiceURL(),
                  "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                  "oauth_token=fake_policy_token&request=policy",
                  std::move(request_proto));
  EXPECT_EQ(response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(response.AssertValidProto());
  ASSERT_EQ(response.proto().policy_response().responses_size(), 1);

  em::PolicyData policy_data;
  ASSERT_TRUE(policy_data.ParseFromString(
      response.proto().policy_response().responses(0).policy_data()));
  EXPECT_EQ(policy_data.policy_type(), kPolicyType);
  em::ExternalPolicyData external_policy_data;
  ASSERT_TRUE(external_policy_data.ParseFromString(policy_data.policy_value()));
  GURL download_url(external_policy_data.download_url());
  ASSERT_TRUE(download_url.is_valid());

  Response download_response =
      SendRequest(fake_dmserver.GetServiceURL(), download_url.PathForRequest());
  EXPECT_EQ(download_response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(download_response.AssertText());
  EXPECT_THAT(download_response.text(), base::test::IsJson(R"({
    "ShorthandPolicy": {"Value": 123},
    "ExplicitPolicy": {"Value": true, "Level": "Recommended"}
  })"));
}

TEST_F(FakeDMServerTest, ReadPolicyBlobFileWithInvalidExtensionIdFails) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  // Non [a-p] character 'z' in extension id.
  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "policy_user": "tast-user@managedchrome.com",
      "machine-extensions": {
        "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz": {
          "SomePolicy": "val"
        }
      }
    }
  )"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, ParseFlagsUnknownSwitch) {
  FakeServerConfig config;

  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII("policy_path", "/tmp/x.json");
    EXPECT_EQ(ParseFlags(command_line, config), ParseFlagsResult::kError);
  }
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII("policy-blob-path", "/tmp/x.json");
    EXPECT_EQ(ParseFlags(command_line, config), ParseFlagsResult::kSuccess);
    EXPECT_EQ(config.policy_blob_path, "/tmp/x.json");
  }
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch("help");
    EXPECT_EQ(ParseFlags(command_line, config), ParseFlagsResult::kHelp);
  }
}

TEST_F(FakeDMServerTest, ParseFlagsFcmPort) {
  {
    FakeServerConfig config;
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    ASSERT_EQ(ParseFlags(command_line, config), ParseFlagsResult::kSuccess);
    // Chrome connects to this port when it uses fake_dmserver. ChromeOS does
    // not connect to it.
    EXPECT_EQ(config.fcm_port,
              BUILDFLAG(IS_CHROMEOS) ? std::nullopt : std::optional(6113));
  }
  {
    FakeServerConfig config;
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII("fcm-port", "0");
    ASSERT_EQ(ParseFlags(command_line, config), ParseFlagsResult::kSuccess);
    EXPECT_EQ(config.fcm_port, std::optional(0));
  }
  {
    FakeServerConfig config;
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII("fcm-port", "1234");
    ASSERT_EQ(ParseFlags(command_line, config), ParseFlagsResult::kSuccess);
    EXPECT_EQ(config.fcm_port, std::optional(1234));
  }
  {
    FakeServerConfig config;
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch("no-fcm");
    ASSERT_EQ(ParseFlags(command_line, config), ParseFlagsResult::kSuccess);
    EXPECT_EQ(config.fcm_port, std::nullopt);
  }
  {
    FakeServerConfig config;
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch("no-fcm");
    command_line.AppendSwitchASCII("fcm-port", "0");
    EXPECT_EQ(ParseFlags(command_line, config), ParseFlagsResult::kError);
  }
}

TEST_F(FakeDMServerTest, WriteURLToPipeOutputsFcmPort) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  int fds[2];
  ASSERT_EQ(pipe(fds), 0);
  base::ScopedFD read_fd(fds[0]);
  base::ScopedFD write_fd(fds[1]);

  EXPECT_TRUE(fake_dmserver.WriteURLToPipe(std::move(write_fd)));

  std::string buffer(1024, '\0');
  ssize_t bytes_read = read(read_fd.get(), buffer.data(), buffer.size());
  ASSERT_GT(bytes_read, 0);
  buffer.resize(static_cast<size_t>(bytes_read));

  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(buffer, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(dict.has_value());

  GURL service_url = fake_dmserver.GetServiceURL();
  const std::string* host = dict->FindString("host");
  ASSERT_TRUE(host);
  EXPECT_EQ(*host, service_url.GetHost());
  EXPECT_EQ(dict->FindInt("port"), service_url.IntPort());
  EXPECT_EQ(dict->FindInt("fcm_port"), fake_dmserver.fcm_server()->port());
  const std::string* realtime_reporting_url =
      dict->FindString("realtime_reporting_url");
  ASSERT_TRUE(realtime_reporting_url);
  EXPECT_EQ(*realtime_reporting_url,
            fake_dmserver.GetRealtimeReportingURL().spec());
}

TEST_F(FakeDMServerTest, IssueRemoteCommandsSendFcmMessages) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  ASSERT_TRUE(fake_dmserver.StartFakeServer());
  auto client =
      ConnectAndLoginFcmClient(fake_dmserver.fcm_server()->port(), 99999);
  ASSERT_TRUE(client);

  em::RemoteCommand command;
  command.set_type(em::RemoteCommand::DEVICE_REMOTE_POWERWASH);
  for (const auto& [scope, type] :
       {std::pair(policy::RemoteCommandScope::kBrowser,
                  kBrowserRemoteCommandInvalidationType),
        std::pair(policy::RemoteCommandScope::kProfile,
                  kProfileRemoteCommandInvalidationType)}) {
    fake_dmserver.remote_commands_state()->AddPendingRemoteCommand(command,
                                                                   scope);
    mcs_proto::DataMessageStanza stanza;
    ASSERT_TRUE(ReadFcmPacket(*client, gcm::kDataMessageStanzaTag, &stanza));
    ExpectFcmProject(stanza, invalidation::kCriticalInvalidationsProjectNumber);
    EXPECT_EQ(GetStanzaType(stanza), type);
  }
}

TEST_F(FakeDMServerTest, AdminApiPoliciesSendFcmMessages) {
  ASSERT_TRUE(base::WriteFile(policy_blob_path_, "{}"));
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  ASSERT_TRUE(fake_dmserver.StartFakeServer());
  const GURL url = fake_dmserver.GetServiceURL();

  // Without connected clients, invalidation succeeds with sent=false.
  Response no_client_resp = SendRequest(url, "/admin/api/policies/invalidate",
                                        "", "application/json");
  EXPECT_EQ(no_client_resp.status, net::HTTP_OK);
  EXPECT_THAT(std::get<std::string>(no_client_resp.body),
              base::test::IsJson(R"({"success": true, "sent": false})"));

  auto client =
      ConnectAndLoginFcmClient(fake_dmserver.fcm_server()->port(), 88888);
  ASSERT_TRUE(client);

  Response sent_resp = SendRequest(url, "/admin/api/policies/invalidate", "",
                                   "application/json");
  EXPECT_EQ(sent_resp.status, net::HTTP_OK);
  EXPECT_THAT(std::get<std::string>(sent_resp.body),
              base::test::IsJson(R"({"success": true, "sent": true})"));
  ExpectPolicyInvalidationStanzas(*client);

  EXPECT_EQ(
      SendRequest(url, "/admin/api/policies", "not json", "application/json")
          .status,
      net::HTTP_BAD_REQUEST);
  constexpr char kPolicies[] =
      "// comment\n"
      R"({"policy_user": "user@example.com", "managed_users": ["*"]})";
  EXPECT_EQ(
      SendRequest(url, "/admin/api/policies", kPolicies, "application/json")
          .status,
      net::HTTP_OK);
  ExpectPolicyInvalidationStanzas(*client);
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(policy_blob_path_, &contents));
  EXPECT_EQ(contents, kPolicies);

  // The watcher event for the write above must not send an invalidation.
  WaitForPolicyFileChange();
  debounce_runner_->FastForwardUntilNoTasksRemain();
  ExpectNextStanzaIsMarker(fake_dmserver, *client);
}

TEST_F(FakeDMServerTest, AdminApiRejectsInvalidPolicies) {
  constexpr char kValidPolicies[] = R"({"policy_user": "user@example.com"})";
  ASSERT_TRUE(base::WriteFile(policy_blob_path_, kValidPolicies));
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  ASSERT_TRUE(fake_dmserver.StartFakeServer());

  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(), "/admin/api/policies",
                        R"({"machine-extensions": {"bad_id": {}}})",
                        "application/json")
                .status,
            net::HTTP_BAD_REQUEST);
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(policy_blob_path_, &contents));
  EXPECT_EQ(contents, kValidPolicies);
}

TEST_F(FakeDMServerTest, AdminApiFixesInvalidPolicyFile) {
  ASSERT_TRUE(base::WriteFile(policy_blob_path_, "not json"));
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  ASSERT_TRUE(fake_dmserver.StartFakeServer());
  const GURL url = fake_dmserver.GetServiceURL();
  constexpr char kRegisterPath[] =
      "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
      "oauth_token=fake_policy_token&request=register";
  ASSERT_EQ(SendRequest(url, kRegisterPath).status,
            net::HTTP_INTERNAL_SERVER_ERROR);

  Response invalidate_resp = SendRequest(url, "/admin/api/policies/invalidate",
                                         "", "application/json");
  EXPECT_EQ(invalidate_resp.status, net::HTTP_BAD_REQUEST);
  EXPECT_THAT(std::get<std::string>(invalidate_resp.body),
              base::test::IsJson(R"({"error": "Invalid policy file"})"));

  EXPECT_EQ(SendRequest(url, "/admin/api/policies",
                        R"({"managed_users": ["*"],
                            "policy_user": "user@example.com"})",
                        "application/json")
                .status,
            net::HTTP_OK);
  EXPECT_EQ(SendRequest(url, kRegisterPath).status, net::HTTP_OK);
}

TEST_F(FakeDMServerTest, PolicyFileChangesAreDebounced) {
  ASSERT_TRUE(base::WriteFile(policy_blob_path_, "{}"));
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  ASSERT_TRUE(fake_dmserver.StartFakeServer());
  auto client =
      ConnectAndLoginFcmClient(fake_dmserver.fcm_server()->port(), 11111);
  ASSERT_TRUE(client);

  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(base::WriteFile(
        policy_blob_path_,
        base::StringPrintf(R"({"policy_user": "user%d@example.com"})", i)));
  }
  WaitForPolicyFileChange();

  debounce_runner_->FastForwardBy(kPolicyFileDebounceDelay -
                                  base::Milliseconds(1));
  ExpectNextStanzaIsMarker(fake_dmserver, *client);

  debounce_runner_->FastForwardUntilNoTasksRemain();
  ExpectPolicyInvalidationStanzas(*client);
  ExpectNextStanzaIsMarker(fake_dmserver, *client);
}

TEST_F(FakeDMServerTest, StartFakeServerWithOccupiedFcmPort) {
  FakeDMServer server1(policy_blob_path_.MaybeAsASCII(),
                       client_state_path_.MaybeAsASCII(),
                       /*grpc_unix_socket_path=*/"", debounce_runner_);
  ASSERT_TRUE(server1.StartFakeServer(/*port=*/0, /*fcm_port=*/0));
  int occupied_fcm_port = server1.fcm_server()->port();
  ASSERT_GT(occupied_fcm_port, 0);

  FakeDMServer server2(policy_blob_path_.MaybeAsASCII(),
                       client_state_path_.MaybeAsASCII(),
                       /*grpc_unix_socket_path=*/"", debounce_runner_);
  EXPECT_TRUE(server2.StartFakeServer(/*port=*/0, occupied_fcm_port));
  EXPECT_EQ(server2.fcm_server()->port(), 0);
  EXPECT_EQ(SendRequest(server2.GetServiceURL(), "/test/ping").status,
            net::HTTP_OK);
}

TEST_F(FakeDMServerTest, StartFakeServerWithFcmDisabled) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             /*grpc_unix_socket_path=*/"", debounce_runner_);
  EXPECT_TRUE(fake_dmserver.StartFakeServer(/*port=*/0, std::nullopt));
  EXPECT_EQ(fake_dmserver.fcm_server()->port(), 0);
}

TEST_F(FakeDMServerTest, WriteDesktopReportRequestAsTextproto) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  em::DeviceManagementRequest request_proto;
  em::ChromeDesktopReportRequest* desktop_report =
      request_proto.mutable_chrome_desktop_report_request();
  em::BrowserReport* browser_report = desktop_report->mutable_browser_report();
  browser_report->set_browser_version("130.0.6723.58");
  browser_report->set_channel(em::Channel::CHANNEL_STABLE);
  browser_report->set_executable_path("/opt/google/chrome/chrome");

  em::OSReport* os_report = desktop_report->mutable_os_report();
  os_report->set_name("Linux");
  os_report->set_arch("x86_64");
  os_report->set_version("6.6.0");

  Response response = SendRequest(
      fake_dmserver.GetServiceURL(),
      base::StringPrintf("/?apptype=Chrome&deviceid=fake_device_id&"
                         "devicetype=2&request=%s",
                         policy::dm_protocol::kValueRequestChromeDesktopReport),
      std::move(request_proto));
  EXPECT_EQ(response.status, net::HTTP_OK);

  base::FileEnumerator enumerator(
      client_state_path_.DirName(),
      /*recursive=*/false, base::FileEnumerator::FILES,
      FILE_PATH_LITERAL("desktop_report_*.textproto"));
  base::FilePath report_file = enumerator.Next();
  ASSERT_FALSE(report_file.empty());
  EXPECT_THAT(
      report_file.BaseName().MaybeAsASCII(),
      testing::MatchesRegex(R"(desktop_report_[0-9]{14}_[0-9]+\.textproto)"));
  EXPECT_TRUE(enumerator.Next().empty());

  std::string file_content;
  ASSERT_TRUE(base::ReadFileToString(report_file, &file_content));
  EXPECT_EQ(file_content,
            "chrome_desktop_report_request {\n"
            "  browser_report {\n"
            "    browser_version: \"130.0.6723.58\"\n"
            "    channel: CHANNEL_STABLE\n"
            "    executable_path: \"/opt/google/chrome/chrome\"\n"
            "  }\n"
            "  os_report {\n"
            "    name: \"Linux\"\n"
            "    arch: \"x86_64\"\n"
            "    version: \"6.6.0\"\n"
            "  }\n"
            "}\n");
}

TEST_F(FakeDMServerTest, WriteProfileReportRequestAsTextproto) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  em::DeviceManagementRequest request_proto;
  em::ChromeProfileReportRequest* profile_report =
      request_proto.mutable_chrome_profile_report_request();
  em::BrowserReport* browser_report = profile_report->mutable_browser_report();
  browser_report->set_browser_version("130.0.6723.58");
  browser_report->set_channel(em::Channel::CHANNEL_STABLE);

  Response response = SendRequest(
      fake_dmserver.GetServiceURL(),
      base::StringPrintf("/?apptype=Chrome&deviceid=fake_device_id&"
                         "devicetype=2&request=%s",
                         policy::dm_protocol::kValueRequestChromeProfileReport),
      std::move(request_proto));
  EXPECT_EQ(response.status, net::HTTP_OK);

  base::FileEnumerator enumerator(
      client_state_path_.DirName(),
      /*recursive=*/false, base::FileEnumerator::FILES,
      FILE_PATH_LITERAL("profile_report_*.textproto"));
  base::FilePath report_file = enumerator.Next();
  ASSERT_FALSE(report_file.empty());
  EXPECT_THAT(
      report_file.BaseName().MaybeAsASCII(),
      testing::MatchesRegex(R"(profile_report_[0-9]{14}_[0-9]+\.textproto)"));
  EXPECT_TRUE(enumerator.Next().empty());

  std::string file_content;
  ASSERT_TRUE(base::ReadFileToString(report_file, &file_content));
  EXPECT_EQ(file_content,
            "chrome_profile_report_request {\n"
            "  browser_report {\n"
            "    browser_version: \"130.0.6723.58\"\n"
            "    channel: CHANNEL_STABLE\n"
            "  }\n"
            "}\n");
}

TEST_F(FakeDMServerTest, WriteInvalidReportRequestDoesNotCrash) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  EXPECT_TRUE(fake_dmserver.StartFakeServer());

  std::string request_url = base::StringPrintf(
      "http://%s:%s/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
      "request=%s",
      fake_dmserver.GetServiceURL().GetHost().c_str(),
      fake_dmserver.GetServiceURL().GetPort().c_str(),
      policy::dm_protocol::kValueRequestChromeDesktopReport);

  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->url = GURL(request_url);

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->AttachStringForUpload("invalid proto bytes",
                                    "application/x-protobuffer");
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>();

  base::test::TestFuture<std::optional<std::string>> test_future;
  url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(), test_future.GetCallback());
  EXPECT_FALSE(test_future.Take().has_value());
  EXPECT_EQ(url_loader->ResponseInfo()->headers->response_code(),
            net::HTTP_BAD_REQUEST);

  base::FileEnumerator enumerator(client_state_path_.DirName(),
                                  /*recursive=*/false,
                                  base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("desktop_report_*"));
  EXPECT_TRUE(enumerator.Next().empty());
}

TEST_F(FakeDMServerTest, WriteRealtimeReportFiles) {
  FakeDMServer fake_dmserver = CreateFakeDMServer();
  ASSERT_TRUE(fake_dmserver.StartFakeServer());

  ::chrome::cros::reporting::proto::UploadEventsRequest request;
  request.add_events()->set_event_id("event_1");
  request.add_events()->set_event_id("event_2");
  EXPECT_EQ(
      SendRequest(fake_dmserver.GetServiceURL(), "/realtime_reporting",
                  request.SerializeAsString(), "application/x-protobuffer")
          .status,
      net::HTTP_OK);

  // Each event has its own .pb and .textproto files.
  EXPECT_EQ(CountDataFiles(FILE_PATH_LITERAL("realtime_*.pb")), 2u);
  EXPECT_EQ(CountDataFiles(FILE_PATH_LITERAL("realtime_*.textproto")), 2u);
}

TEST_F(FakeDMServerTest, PersistedReportsAreLoadedAtStartup) {
  auto send_report = [&](FakeDMServer& server, const char* request_type,
                         em::DeviceManagementRequest request_proto) {
    ASSERT_EQ(SendRequest(server.GetServiceURL(),
                          base::StringPrintf(
                              "/?apptype=Chrome&deviceid=dev1&devicetype=2&"
                              "request=%s",
                              request_type),
                          std::move(request_proto))
                  .status,
              net::HTTP_OK);
  };
  auto send_desktop_report = [&](FakeDMServer& server,
                                 const std::string& version) {
    em::DeviceManagementRequest request_proto;
    request_proto.mutable_chrome_desktop_report_request()
        ->mutable_browser_report()
        ->set_browser_version(version);
    send_report(server, policy::dm_protocol::kValueRequestChromeDesktopReport,
                std::move(request_proto));
  };
  auto send_profile_report = [&](FakeDMServer& server,
                                 const std::string& version) {
    em::DeviceManagementRequest request_proto;
    request_proto.mutable_chrome_profile_report_request()
        ->mutable_browser_report()
        ->set_browser_version(version);
    send_report(server, policy::dm_protocol::kValueRequestChromeProfileReport,
                std::move(request_proto));
  };
  {
    FakeDMServer fake_dmserver = CreateFakeDMServer();
    ASSERT_TRUE(fake_dmserver.StartFakeServer());
    // Send multiple reports from the same device in the same second to verify
    // persisted files do not overwrite each other.
    send_desktop_report(fake_dmserver, "130.0.6723.58");
    send_desktop_report(fake_dmserver, "131.0.6778.10");
    send_profile_report(fake_dmserver, "130.0.6723.58");
    send_profile_report(fake_dmserver, "131.0.6778.10");

    ::chrome::cros::reporting::proto::UploadEventsRequest realtime_request;
    realtime_request.add_events()->set_event_id("event_1");
    realtime_request.add_events()->set_event_id("event_2");
    ASSERT_EQ(SendRequest(fake_dmserver.GetServiceURL(), "/realtime_reporting",
                          realtime_request.SerializeAsString(),
                          "application/x-protobuffer")
                  .status,
              net::HTTP_OK);
  }

  EXPECT_EQ(CountDataFiles(FILE_PATH_LITERAL("desktop_report_*.pb")), 2u);
  EXPECT_EQ(CountDataFiles(FILE_PATH_LITERAL("desktop_report_*.textproto")),
            2u);
  EXPECT_EQ(CountDataFiles(FILE_PATH_LITERAL("profile_report_*.pb")), 2u);
  EXPECT_EQ(CountDataFiles(FILE_PATH_LITERAL("profile_report_*.textproto")),
            2u);
  EXPECT_EQ(CountDataFiles(FILE_PATH_LITERAL("realtime_*.pb")), 2u);
  EXPECT_EQ(CountDataFiles(FILE_PATH_LITERAL("realtime_*.textproto")), 2u);

  FakeDMServer fake_dmserver = CreateFakeDMServer();
  ASSERT_TRUE(fake_dmserver.StartFakeServer());

  auto extract_versions = [](const auto& entries) {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& entry : entries) {
      result.emplace_back(entry.device_id,
                          entry.report.browser_report().browser_version());
    }
    return result;
  };
  EXPECT_THAT(
      extract_versions(fake_dmserver.report_storage()->GetBrowserReports()),
      testing::UnorderedElementsAre(std::make_pair("dev1", "130.0.6723.58"),
                                    std::make_pair("dev1", "131.0.6778.10")));
  EXPECT_THAT(
      extract_versions(fake_dmserver.report_storage()->GetProfileReports()),
      testing::UnorderedElementsAre(std::make_pair("dev1", "130.0.6723.58"),
                                    std::make_pair("dev1", "131.0.6778.10")));

  std::vector<std::string> event_ids;
  for (const auto& event :
       fake_dmserver.report_storage()->GetRealtimeEvents()) {
    event_ids.push_back(event.event_id());
  }
  EXPECT_THAT(event_ids, testing::UnorderedElementsAre("event_1", "event_2"));
}

}  // namespace fakedms
