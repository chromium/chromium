// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/oobe_quick_start/connectivity/target_fido_controller.h"

#include "base/json/json_reader.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash::quick_start {

namespace {
const char kChallengeBase64Url[] = "testchallenge";
const char kTestOrigin[] = "https://google.com";
}  // namespace

class TargetFidoControllerTest : public testing::Test {
 public:
  TargetFidoControllerTest() = default;
  TargetFidoControllerTest(TargetFidoControllerTest&) = delete;
  TargetFidoControllerTest& operator=(TargetFidoControllerTest&) = delete;
  ~TargetFidoControllerTest() override = default;

  // TODO(b/234655072): Pass in FakeNearbyConnectionsManager when available.
  void SetUp() override { CreateFidoController(nullptr); }

  void CreateFidoController(
      NearbyConnectionsManager* nearby_connections_manager) {
    fido_controller_ =
        std::make_unique<TargetFidoController>(nearby_connections_manager);
  }

  void OnRequestAssertion(bool success) {
    request_assertion_callback_called_ = true;
    request_assertion_success_ = success;
  }

  std::string CreateClientDataJson(std::string challenge_b64url,
                                   url::Origin origin) {
    return fido_controller_->CreateClientDataJson(origin, challenge_b64url);
  }

  cbor::Value GenerateGetAssertionRequest(std::string challenge_b64url) {
    return fido_controller_->GenerateGetAssertionRequest(challenge_b64url);
  }

  std::vector<uint8_t> CBOREncodeGetAssertionRequest(cbor::Value request) {
    return fido_controller_->CBOREncodeGetAssertionRequest(std::move(request));
  }

 protected:
  std::unique_ptr<TargetFidoController> fido_controller_;
  bool request_assertion_callback_called_ = false;
  bool request_assertion_success_ = false;
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::WeakPtrFactory<TargetFidoControllerTest> weak_ptr_factory_{this};
};

TEST_F(TargetFidoControllerTest,
       OnRequestAssertion_CallbackRegisteredAndCalled) {
  fido_controller_->RequestAssertion(
      kChallengeBase64Url,
      base::BindOnce(&TargetFidoControllerTest::OnRequestAssertion,
                     weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(request_assertion_callback_called_);
  EXPECT_TRUE(request_assertion_success_);
}

TEST_F(TargetFidoControllerTest, CreateClientDataJson) {
  url::Origin test_origin = url::Origin::Create(GURL(kTestOrigin));
  std::string client_data_json =
      CreateClientDataJson(kChallengeBase64Url, test_origin);
  absl::optional<base::Value> parsed_json =
      base::JSONReader::Read(client_data_json);
  ASSERT_TRUE(parsed_json);
  ASSERT_TRUE(parsed_json->is_dict());
  base::Value::Dict& parsed_json_dict = parsed_json.value().GetDict();
  EXPECT_EQ(*parsed_json_dict.FindString("type"), "webauthn.get");
  EXPECT_EQ(*parsed_json_dict.FindString("challenge"), kChallengeBase64Url);
  EXPECT_EQ(*parsed_json_dict.FindString("origin"), kTestOrigin);
  EXPECT_EQ(parsed_json_dict.FindBool("crossOrigin"), false);
}

TEST_F(TargetFidoControllerTest, GenerateGetAssertionRequest_ValidChallenge) {
  cbor::Value request = GenerateGetAssertionRequest(kChallengeBase64Url);
  ASSERT_TRUE(request.is_map());
  const cbor::Value::MapValue& request_map = request.GetMap();
  // CBOR Index 0x01 stores the relying_party_id for the GetAssertionRequest.
  EXPECT_EQ(request_map.find(cbor::Value(0x01))->second.GetString(),
            "google.com");
  // CBOR Index 0x05 stores the options for the GetAssertionRequest.
  const cbor::Value::MapValue& options_map =
      request_map.find(cbor::Value(0x05))->second.GetMap();
  // CBOR key "uv" stores the userVerification bit for the options.
  EXPECT_TRUE(options_map.find(cbor::Value("uv"))->second.GetBool());
  // CBOR key "up" stores the userPresence bit for the options.
  EXPECT_TRUE(options_map.find(cbor::Value("up"))->second.GetBool());
  EXPECT_TRUE(
      request_map.find(cbor::Value(0x02))->second.GetBytestring().size() > 0);
}

TEST_F(TargetFidoControllerTest, CBOREncodeGetAssertionRequest) {
  cbor::Value request = GenerateGetAssertionRequest(kChallengeBase64Url);
  std::vector<uint8_t> cbor_encoded_request =
      CBOREncodeGetAssertionRequest(std::move(request));
  absl::optional<cbor::Value> cbor;
  const base::span<const uint8_t> ctap_request_span =
      base::make_span(cbor_encoded_request);
  cbor = cbor::Reader::Read(ctap_request_span.subspan(1));
  ASSERT_TRUE(cbor);
  ASSERT_TRUE(cbor->is_map());
  const cbor::Value::MapValue& cbor_map = cbor->GetMap();
  // CBOR Index 0x01 stores the relying_party_id for the GetAssertionRequest.
  EXPECT_EQ(cbor_map.find(cbor::Value(0x01))->second.GetString(), "google.com");
}

TEST_F(TargetFidoControllerTest, GenerateGetAssertionReqeust_EmptyChallenge) {
  fido_controller_->RequestAssertion(
      "", base::BindOnce(&TargetFidoControllerTest::OnRequestAssertion,
                         weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(request_assertion_callback_called_);
  EXPECT_FALSE(request_assertion_success_);
}

}  // namespace ash::quick_start