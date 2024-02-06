// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/fake_dmserver.h"

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
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

  // TODO(b/240445061): Check response content to verify the returned policy.
  int SendRequest(const GURL& server_url, const std::string& request_path) {
    std::string request_url =
        base::StringPrintf("http://%s:%s%s", server_url.host().c_str(),
                           server_url.port().c_str(), request_path.c_str());
    std::unique_ptr<network::ResourceRequest> resource_request =
        std::make_unique<network::ResourceRequest>();
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
    resource_request->url = GURL(request_url);
    resource_request->headers.SetHeader(
        "Authorization", "GoogleDMToken token=fake_device_token");

    std::unique_ptr<network::SimpleURLLoader> url_loader =
        network::SimpleURLLoader::Create(std::move(resource_request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();

    base::test::TestFuture<std::unique_ptr<std::string>> test_future;
    url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(), test_future.GetCallback());
    const std::unique_ptr<std::string> response_body = test_future.Take();
    if (response_body) {
      LOG(INFO) << "Response body: " << *response_body;
    }
    LOG(INFO) << "Response headers: "
              << url_loader->ResponseInfo()->headers->raw_headers();
    return url_loader->ResponseInfo()->headers->response_code();
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
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(), "/test/exit"),
            net::HTTP_OK);
}

TEST_F(FakeDMServerTest, HandlePingRequestSucceeds) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(), "/test/ping"),
            net::HTTP_OK);
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

  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=register"),
            net::HTTP_OK);

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
            policy::dm_protocol::kChromeUserPolicyType);
  EXPECT_TRUE(clients[0].state_keys.empty());

  JSONFileValueDeserializer deserializer(client_state_path_);
  int error_code = 0;
  std::string error_msg;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_msg);
  ASSERT_TRUE(value);
  const base::Value::Dict* state_dict = value->GetIfDict();
  ASSERT_TRUE(state_dict);
  ASSERT_EQ(state_dict->size(), 1u);
  const base::Value::Dict* client_dict = state_dict->FindDict("fake_device_id");
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

  const base::Value::List* allowed_policy_types =
      client_dict->FindList("allowed_policy_types");
  ASSERT_TRUE(allowed_policy_types);
  ASSERT_EQ(allowed_policy_types->size(), 1u);
  EXPECT_EQ((*allowed_policy_types)[0].GetString(),
            policy::dm_protocol::kChromeUserPolicyType);

  const base::Value::List* state_keys = client_dict->FindList("state_keys");
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
                        "oauth_token=fake_policy_token&request=register"),
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
                        "oauth_token=fake_policy_token&request=register"),
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
                        "oauth_token=fake_policy_token&request=register"),
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
                        "oauth_token=fake_policy_token&request=register"),
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
                        "oauth_token=fake_policy_token&request=register"),
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
                        "oauth_token=fake_policy_token&request=register"),
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
                        "oauth_token=fake_policy_token&request=register"),
            net::HTTP_INTERNAL_SERVER_ERROR);
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
                        "oauth_token=fake_policy_token&request=register"),
            net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(FakeDMServerTest, HandlePolicyRequestSucceeds) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

  ASSERT_TRUE(base::WriteFile(
      policy_blob_path_,
      R"(
    {
      "managed_users" : [ "*" ],
      "policy_user" : "tast-user@managedchrome.com",
      "policies" : [
        {
          "policy_type" : "google/chromeos/user", "value" : "uhMCEAE="
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
                        "oauth_token=fake_policy_token&request=policy"),
            net::HTTP_OK);

  std::string user_policy_payload =
      fake_dmserver.policy_storage()->GetPolicyPayload("google/chromeos/user",
                                                       "");
  std::string user_policy_output = base::Base64Encode(user_policy_payload);
  EXPECT_EQ(user_policy_output, "uhMCEAE=");

  std::string device_policy_payload =
      fake_dmserver.policy_storage()->GetPolicyPayload("google/chromeos/device",
                                                       "");
  std::string device_policy_output = base::Base64Encode(device_policy_payload);
  EXPECT_EQ(device_policy_output,
            "qgFSCikSJWRlZmF1bHRNZ3NTZXRCeVRhc3RAbWFuYWdlZGNocm9tZS5jb20YABIlZG"
            "VmYXVsdE1nc1NldEJ5VGFzdEBtYW5hZ2VkY2hyb21lLmNvbQ==");

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
                        "oauth_token=fake_policy_token&request=policy"),
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
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy"),
            net::HTTP_OK);

  std::string policy_data = fake_dmserver.policy_storage()->GetPolicyPayload(
      "google/chrome/extension", "ibdnofdagboejmpijdiknapcihkomkki");
  ASSERT_FALSE(policy_data.empty());
  enterprise_management::ExternalPolicyData data;
  ASSERT_TRUE(data.ParseFromString(policy_data));
  EXPECT_EQ(data.secure_hash(), kSHA256HashForExtensionPolicyPayload);
  // TODO(b/240445061): Write an integration test that issues a request to the
  // returned URL and verifies that it returns correct policy.
  ASSERT_TRUE(data.has_download_url());

  std::string extension_policy_payload =
      fake_dmserver.policy_storage()->GetExternalPolicyPayload(
          "google/chrome/extension", "ibdnofdagboejmpijdiknapcihkomkki");
  EXPECT_EQ(extension_policy_payload, kRawExtensionPolicyPayload);
}

TEST_F(FakeDMServerTest, ReadPolicyBlobFileWithWrongJSONDataFails) {
  FakeDMServer fake_dmserver(policy_blob_path_.MaybeAsASCII(),
                             client_state_path_.MaybeAsASCII(),
                             grpc_unix_socket_uri_);
  EXPECT_TRUE(fake_dmserver.Start());

  ASSERT_TRUE(base::WriteFile(policy_blob_path_, "wrong data"));
  EXPECT_EQ(SendRequest(fake_dmserver.GetServiceURL(),
                        "/?apptype=Chrome&deviceid=fake_device_id&devicetype=2&"
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
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
                        "oauth_token=fake_policy_token&request=policy"),
            net::HTTP_INTERNAL_SERVER_ERROR);
}

}  // namespace fakedms
