// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_hash_data.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

TEST(PasswordHashDataTest, CalculatePasswordHash) {
  const char* kPlainText[] = {"", "password", "password", "secret"};
  const char* kSalt[] = {"", "salt", "123", "456"};

  constexpr uint64_t kExpectedHash[] = {
      UINT64_C(0x1c610a7950), UINT64_C(0x1927dc525e), UINT64_C(0xf72f81aa6),
      UINT64_C(0x3645af77f),
  };

  static_assert(std::size(kPlainText) == std::size(kSalt),
                "Arrays must have the same size");
  static_assert(std::size(kPlainText) == std::size(kExpectedHash),
                "Arrays must have the same size");

  for (size_t i = 0; i < std::size(kPlainText); ++i) {
    SCOPED_TRACE(i);
    std::u16string text = base::UTF8ToUTF16(kPlainText[i]);
    EXPECT_EQ(kExpectedHash[i], CalculatePasswordHash(text, kSalt[i]));
  }
}

}  // namespace
}  // namespace password_manager
