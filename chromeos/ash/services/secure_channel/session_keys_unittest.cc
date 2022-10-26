// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/session_keys.h"

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::secure_channel {

namespace {

// Values generated using the Android implementation.
const char kSessionKeyHex[] =
    "f611126a04302551ac1e8ed512952ee287a1d2561e2a2c72e7bf1ebe4bdc74ce";
const char kInitiatorKeyHex[] =
    "787ec48783f0a1f9fb9c5bc0230c2e7f45b8783acf8c9bd1c63242df9da31999";
const char kResponderKeyHex[] =
    "a366ec1f9cf327b69c341211216545cc302379078229eae78b43d60c110a6fba";

}  // namespace

class SecureChannelSessionKeysTest : public testing::Test {
 public:
  SecureChannelSessionKeysTest(const SecureChannelSessionKeysTest&) = delete;
  SecureChannelSessionKeysTest& operator=(const SecureChannelSessionKeysTest&) =
      delete;

 protected:
  SecureChannelSessionKeysTest() {}
};

TEST_F(SecureChannelSessionKeysTest, GenerateKeys) {
  std::string session_key;
  ASSERT_TRUE(base::HexStringToString(kSessionKeyHex, &session_key));

  std::string initiator_key;
  ASSERT_TRUE(base::HexStringToString(kInitiatorKeyHex, &initiator_key));

  std::string responder_key;
  ASSERT_TRUE(base::HexStringToString(kResponderKeyHex, &responder_key));

  SessionKeys session_keys(session_key);
  EXPECT_EQ(initiator_key, session_keys.initiator_encode_key());
  EXPECT_EQ(responder_key, session_keys.responder_encode_key());
}

}  // namespace ash::secure_channel
