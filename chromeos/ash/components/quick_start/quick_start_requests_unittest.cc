// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_requests.h"
#include "base/json/json_reader.h"
#include "components/cbor/reader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ash::quick_start::requests::CBOREncodeGetAssertionRequest;
using ash::quick_start::requests::CreateFidoClientDataJson;
using ash::quick_start::requests::GenerateGetAssertionRequest;

namespace {
const char kChallengeBase64Url[] = "testchallenge";
const char kTestOrigin[] = "https://google.com";
const char kCtapRequestType[] = "webauthn.get";
}  // namespace

class QuickStartRequestTest : public testing::Test {
 public:
  QuickStartRequestTest() = default;
  QuickStartRequestTest(QuickStartRequestTest&) = delete;
  QuickStartRequestTest& operator=(QuickStartRequestTest&) = delete;
  ~QuickStartRequestTest() override = default;

 protected:
  void SetUp() override {}
};

TEST_F(QuickStartRequestTest, CreateFidoClientDataJson) {
  url::Origin test_origin = url::Origin::Create(GURL(kTestOrigin));
  std::string client_data_json =
      CreateFidoClientDataJson(test_origin, kChallengeBase64Url);
  absl::optional<base::Value> parsed_json =
      base::JSONReader::Read(client_data_json);
  ASSERT_TRUE(parsed_json);
  ASSERT_TRUE(parsed_json->is_dict());
  base::Value::Dict& parsed_json_dict = parsed_json.value().GetDict();
  EXPECT_EQ(*parsed_json_dict.FindString("type"), kCtapRequestType);
  EXPECT_EQ(*parsed_json_dict.FindString("challenge"), kChallengeBase64Url);
  EXPECT_EQ(*parsed_json_dict.FindString("origin"), kTestOrigin);
  EXPECT_FALSE(parsed_json_dict.FindBool("crossOrigin").value());
}

TEST_F(QuickStartRequestTest, GenerateGetAssertionRequest_ValidChallenge) {
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

TEST_F(QuickStartRequestTest, CBOREncodeGetAssertionRequest) {
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
