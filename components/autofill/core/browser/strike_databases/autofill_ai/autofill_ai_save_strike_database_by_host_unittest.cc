// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_databases/autofill_ai/autofill_ai_save_strike_database_by_host.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// Tests that retrieving hosts from ids works correctly.
TEST(GetIdForSaveStrikeDatabaseByHostTest, GetHostFromId) {
  constexpr char kSomeHost[] = "www.passport-page.gov";
  constexpr char kOtherHost[] = "www.whoareyou.com";

  EXPECT_EQ(
      AutofillAiSaveStrikeDatabaseByHostTraits::HostFromId(
          AutofillAiSaveStrikeDatabaseByHost::GetId("passport", kSomeHost)),
      kSomeHost);
  EXPECT_EQ(
      AutofillAiSaveStrikeDatabaseByHostTraits::HostFromId(
          AutofillAiSaveStrikeDatabaseByHost::GetId("passport", kOtherHost)),
      kOtherHost);

  // Invalid id:
  EXPECT_EQ(
      AutofillAiSaveStrikeDatabaseByHostTraits::HostFromId("no-separator"), "");
}

// Tests that different hosts lead to different keys.
TEST(GetIdForSaveStrikeDatabaseByHostTest, DifferentHosts) {
  constexpr char kSomeEntity[] = "passport";

  EXPECT_NE(
      AutofillAiSaveStrikeDatabaseByHost::GetId(kSomeEntity, "www.abc.com"),
      AutofillAiSaveStrikeDatabaseByHost::GetId(kSomeEntity, "www.google.com"));
}

// Tests that different entities lead to different keys.
TEST(GetIdForSaveStrikeDatabaseByHostTest, DifferentEntities) {
  constexpr char kSomeHost[] = "www.passport-page.gov";

  EXPECT_NE(AutofillAiSaveStrikeDatabaseByHost::GetId("passport", kSomeHost),
            AutofillAiSaveStrikeDatabaseByHost::GetId("license", kSomeHost));
}

}  // namespace

}  // namespace autofill
