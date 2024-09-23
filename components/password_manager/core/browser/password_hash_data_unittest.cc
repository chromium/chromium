// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_hash_data.h"

#include <array>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

TEST(PasswordHashDataTest, CalculatePasswordHash) {
  constexpr auto kPlainText =
      std::to_array<const char*>({"", "password", "password", "secret"});
  constexpr auto kSalt = std::to_array<const char*>({"", "salt", "123", "456"});

  constexpr auto kExpectedHash =
      std::to_array<uint64_t>({UINT64_C(0x1c610a7950), UINT64_C(0x1927dc525e),
                               UINT64_C(0xf72f81aa6), UINT64_C(0x3645af77f)});

  static_assert(kPlainText.size() == kSalt.size(),
                "Arrays must have the same size");
  static_assert(kPlainText.size() == kExpectedHash.size(),
                "Arrays must have the same size");

  for (size_t i = 0; i < kPlainText.size(); ++i) {
    SCOPED_TRACE(i);
    std::u16string text = base::UTF8ToUTF16(kPlainText[i]);
    EXPECT_EQ(kExpectedHash[i], CalculatePasswordHash(text, kSalt[i]));
  }
}

}  // namespace
}  // namespace password_manager
