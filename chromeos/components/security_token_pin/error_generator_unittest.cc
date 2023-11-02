// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/security_token_pin/error_generator.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/security_token_pin/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace security_token_pin {

class SecurityTokenPinErrorGeneratorTest : public testing::Test {
 protected:
  SecurityTokenPinErrorGeneratorTest() = default;

  SecurityTokenPinErrorGeneratorTest(
      const SecurityTokenPinErrorGeneratorTest&) = delete;
  SecurityTokenPinErrorGeneratorTest& operator=(
      const SecurityTokenPinErrorGeneratorTest&) = delete;
};

// Tests that an empty message is returned when there's neither an error nor the
// number of attempts left.
TEST_F(SecurityTokenPinErrorGeneratorTest, NoError) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kNone, /*attempts_left=*/-1,
                                 /*accept_input=*/true),
            std::u16string());
}

// Tests the message for the kInvalidPin error.
TEST_F(SecurityTokenPinErrorGeneratorTest, InvalidPin) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kInvalidPin, /*attempts_left=*/-1,
                                 /*accept_input=*/true),
            u"Invalid PIN.");
}

// Tests the message for the kInvalidPuk error.
TEST_F(SecurityTokenPinErrorGeneratorTest, InvalidPuk) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kInvalidPuk, /*attempts_left=*/-1,
                                 /*accept_input=*/true),
            u"Invalid PUK.");
}

// Tests the message for the kMaxAttemptsExceeded error.
TEST_F(SecurityTokenPinErrorGeneratorTest, MaxAttemptsExceeded) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kMaxAttemptsExceeded,
                                 /*attempts_left=*/-1,
                                 /*accept_input=*/false),
            u"Maximum allowed attempts exceeded.");
}

// Tests the message for the kMaxAttemptsExceeded error with the zero number of
// attempts left.
TEST_F(SecurityTokenPinErrorGeneratorTest, MaxAttemptsExceededZeroAttempts) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kMaxAttemptsExceeded,
                                 /*attempts_left=*/0,
                                 /*accept_input=*/false),
            u"Maximum allowed attempts exceeded.");
}

// Tests the message for the kUnknown error.
TEST_F(SecurityTokenPinErrorGeneratorTest, UnknownError) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kUnknown, /*attempts_left=*/-1,
                                 /*accept_input=*/true),
            u"Unknown error.");
}

// Tests the message when the number of attempts left is given.
TEST_F(SecurityTokenPinErrorGeneratorTest, Attempts) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kNone, /*attempts_left=*/1,
                                 /*accept_input=*/true),
            u"1 attempt left");
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kNone, /*attempts_left=*/3,
                                 /*accept_input=*/true),
            u"3 attempts left");
}

// Tests that an empty message is returned when the number of attempts is given
// such that, heuristically, it's too big to be displayed for the user.
TEST_F(SecurityTokenPinErrorGeneratorTest, HiddenAttempts) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kNone, /*attempts_left=*/4,
                                 /*accept_input=*/true),
            std::u16string());
}

// Tests the message for the kInvalidPin error with the number of attempts left.
TEST_F(SecurityTokenPinErrorGeneratorTest, InvalidPinWithAttempts) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kInvalidPin, /*attempts_left=*/1,
                                 /*accept_input=*/true),
            u"Invalid PIN. 1 attempt left");
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kInvalidPin, /*attempts_left=*/3,
                                 /*accept_input=*/true),
            u"Invalid PIN. 3 attempts left");
}

// Tests the message for the kInvalidPin error with such a number of attempts
// left that, heuristically, shouldn't be displayed to the user.
TEST_F(SecurityTokenPinErrorGeneratorTest, InvalidPinWithHiddenAttempts) {
  EXPECT_EQ(GenerateErrorMessage(ErrorLabel::kInvalidPin, /*attempts_left=*/4,
                                 /*accept_input=*/true),
            u"Invalid PIN.");
}

}  // namespace security_token_pin
}  // namespace chromeos
