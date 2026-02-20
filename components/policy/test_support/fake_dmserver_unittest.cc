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
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/embedded_policy_test_server_test_base.h"
#include "components/policy/test_support/policy_storage.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
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

}  // namespace

// TODO(b/239676448): Add missing unittest for Writing to Pipe.
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
    grpc_unix_socket_uri_ = "unix:///tmp/fake_dmserver_grpc.sock";
  }

  Response SendRequest(
      const GURL& server_url,
      const std::string& request_path,
      std::optional<em::DeviceManagementRequest> request_proto = std::nullopt) {
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
    if (request_proto) {
      std::string body;
      CHECK(request_proto->SerializeToString(&body));
      url_loader->AttachStringForUpload(body, "application/x-protobuffer");
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
  std::string grpc_unix_socket_uri_;

 private:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(FakeDMServerTest, HandleExitRequestSucceeds) {
  base::MockOnceCallback<void()> callback;
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_, callback.Get());
  EXPECT_TRUE(fake_dmserver.Start());

  EXPECT_CALL(callback, Run());
  Response response = SendRequest(fake_dmserver.GetServiceURL(), "/test/exit");
  EXPECT_EQ(response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(response.AssertText());
  EXPECT_EQ(response.text(), "Policy Server exited.");
}

TEST_F(FakeDMServerTest, HandlePingRequestSucceeds) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

  Response response = SendRequest(fake_dmserver.GetServiceURL(), "/test/ping");
  EXPECT_EQ(response.status, net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(response.AssertText());
  EXPECT_EQ(response.text(), "Pong.");
}

TEST_F(FakeDMServerTest, HandleRegisterRequestSucceeds) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

  ASSERT_TRUE(base::WriteFile(client_state_path_, "wrong data"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, ReadClientStateFileWithNonDictFileFails) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

  ASSERT_TRUE(base::WriteFile(client_state_path_, R"([ "1", "2" ])"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, GetClientFromValueWithNonDictValueFails) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

  ASSERT_TRUE(base::WriteFile(client_state_path_,
                              R"({ "fake_device_id" : "not dict" })"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, GetClientFromValueWithOnlyDeviceIDFails) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

  ASSERT_TRUE(base::WriteFile(client_state_path_,
                              R"({ "fake_device_id" : { "device_id" : 7 } })"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, GetClientFromValueWithoutStateKeyListFails) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  em::PolicyFetchResponse policy_fetch_response =
      response.proto().policy_response().responses(0);

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, "wrong data"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, ReadPolicyBlobFileWithNonDictFileFails) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"([ "1", "2" ])"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy")
                .status,
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, ReadPolicyBlobFileWithNonDictPoliciesFails) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, R"(
    {
      "policy_user" : "tast-user@managedchrome.com",
      "machine": {
        "AllowDinosaurEasterEgg": true
      },
      "user": {
        "HomepageLocation": "http://example.com"
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

  std::map<std::string, em::PolicyFetchResponse> responses;
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
  }

  {
    em::PolicyData policy_data;
    ASSERT_TRUE(policy_data.ParseFromString(
        responses[policy::dm_protocol::GetChromeUserPolicyType()].policy_data()));
    em::CloudPolicySettings settings;
    ASSERT_TRUE(settings.ParseFromString(policy_data.policy_value()));
    EXPECT_TRUE(settings.has_homepagelocation());
    EXPECT_EQ(settings.homepagelocation().value(), "http://example.com");
  }
}

TEST_F(FakeDMServerTest,
       HandleExtensionInstallPolicyRequestWithJsonFormatSucceeds) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

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

}  // namespace fakedms
