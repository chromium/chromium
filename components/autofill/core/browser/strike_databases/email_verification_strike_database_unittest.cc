// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/email_verification_strike_database.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

namespace autofill {
namespace {

TEST(EmailVerificationStrikeDatabaseTest, GetIdReturnsTwoCharHex) {
  std::string id = EmailVerificationStrikeDatabase::GetId("test@example.com");
  EXPECT_EQ(id.length(), 2u);
  // Verify it's hex.
  EXPECT_TRUE(absl::ascii_isxdigit(static_cast<unsigned char>(id[0])));
  EXPECT_TRUE(absl::ascii_isxdigit(static_cast<unsigned char>(id[1])));
}

TEST(EmailVerificationStrikeDatabaseTest, GetIdIsConsistent) {
  EXPECT_EQ(EmailVerificationStrikeDatabase::GetId("test@example.com"),
            EmailVerificationStrikeDatabase::GetId("test@example.com"));
}

}  // namespace
}  // namespace autofill
