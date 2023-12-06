// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/quick_start/quick_start_requests.h"

#include "components/cbor/reader.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::quick_start::requests::CBOREncodeGetAssertionRequest;
using ash::quick_start::requests::GenerateGetAssertionRequest;

namespace {
const char kTestClientDataString[] = "test_client_data";
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

TEST_F(QuickStartRequestTest, GenerateGetAssertionRequest_ValidChallenge) {
  std::array<uint8_t, crypto::kSHA256Length> client_data_hash;
  crypto::SHA256HashString(kTestClientDataString, client_data_hash.data(),
                           client_data_hash.size());
  cbor::Value request = GenerateGetAssertionRequest(client_data_hash);
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
  EXPECT_GT(request_map.find(cbor::Value(0x02))->second.GetBytestring().size(),
            0UL);
}

TEST_F(QuickStartRequestTest, CBOREncodeGetAssertionRequest) {
  std::array<uint8_t, crypto::kSHA256Length> client_data_hash;
  crypto::SHA256HashString(kTestClientDataString, client_data_hash.data(),
                           client_data_hash.size());
  cbor::Value request = GenerateGetAssertionRequest(client_data_hash);
  std::vector<uint8_t> cbor_encoded_request =
      CBOREncodeGetAssertionRequest(std::move(request));
  std::optional<cbor::Value> cbor;
  const base::span<const uint8_t> ctap_request_span =
      base::make_span(cbor_encoded_request);
  cbor = cbor::Reader::Read(ctap_request_span.subspan(1));
  ASSERT_TRUE(cbor);
  ASSERT_TRUE(cbor->is_map());
  const cbor::Value::MapValue& cbor_map = cbor->GetMap();
  // CBOR Index 0x01 stores the relying_party_id for the GetAssertionRequest.
  EXPECT_EQ(cbor_map.find(cbor::Value(0x01))->second.GetString(), "google.com");
}
