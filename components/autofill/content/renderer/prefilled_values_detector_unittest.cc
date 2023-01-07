// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/prefilled_values_detector.h"

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Ensure that all entries in KnownUsernamePlaceholders() are lowercase because
// the lowercase string of the website is tested against this set.
TEST(PossiblePrefilledUsernameValue, AllLowerCase) {
  for (auto entry : KnownUsernamePlaceholders())
    EXPECT_EQ(entry, base::ToLowerASCII(entry));
}

TEST(PossiblePrefilledUsernameValue, Whitespace) {
  EXPECT_TRUE(PossiblePrefilledUsernameValue(" ", ""));
}

TEST(PossiblePrefilledUsernameValue, EmailAddress) {
  EXPECT_TRUE(PossiblePrefilledUsernameValue("@example.com", "example.com"));
  EXPECT_TRUE(PossiblePrefilledUsernameValue("@EXAMPLE.COM", "example.com"));
  EXPECT_TRUE(PossiblePrefilledUsernameValue(" @example.com", "example.com"));
  EXPECT_TRUE(
      PossiblePrefilledUsernameValue("@mail.example.com", "example.com"));
  EXPECT_FALSE(
      PossiblePrefilledUsernameValue("user@example.com", "example.com"));
  EXPECT_FALSE(PossiblePrefilledUsernameValue("@example.com", "foo.com"));
  EXPECT_FALSE(PossiblePrefilledUsernameValue("@example.com", ""));
  EXPECT_FALSE(PossiblePrefilledUsernameValue("@", "foo.com"));
  EXPECT_FALSE(PossiblePrefilledUsernameValue("@", ""));
}

}  // namespace autofill
