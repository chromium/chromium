// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/quick_start/quick_start_requests.h"

#include "components/cbor/reader.h"
#include "crypto/hash.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::quick_start::requests::CBOREncodeGetAssertionRequest;
using ash::quick_start::requests::GenerateGetAssertionRequest;

namespace {
// Used as a dummy client data hash when constructing test requests.
const std::array<uint8_t, crypto::hash::kSha256Size> kTestClientDataHash = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
};
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
  cbor::Value request = GenerateGetAssertionRequest(kTestClientDataHash);
  ASSERT_TRUE(request.is_map());
  const cbor::Value::MapValue& request_map = request.GetMap();
  // CBOR Index 0x01 stores the relying_party_id for the GetAssertionRequest.
  EXPECT_EQ(request_map.find(cbor::Value(0x01))->second.GetString(),
            "google.com");
  // CBOR Index 0x02 stores the client data hash.
  EXPECT_EQ(base::as_byte_span(
                request_map.find(cbor::Value(0x02))->second.GetBytestring()),
            kTestClientDataHash);
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
  cbor::Value request = GenerateGetAssertionRequest(kTestClientDataHash);
  std::vector<uint8_t> cbor_encoded_request =
      CBOREncodeGetAssertionRequest(std::move(request));
  std::optional<cbor::Value> cbor;
  const base::span<const uint8_t> ctap_request_span =
      base::span(cbor_encoded_request);
  cbor = cbor::Reader::Read(ctap_request_span.subspan<1>());
  ASSERT_TRUE(cbor);
  ASSERT_TRUE(cbor->is_map());
  const cbor::Value::MapValue& cbor_map = cbor->GetMap();
  // CBOR Index 0x01 stores the relying_party_id for the GetAssertionRequest.
  EXPECT_EQ(cbor_map.find(cbor::Value(0x01))->second.GetString(), "google.com");
}
