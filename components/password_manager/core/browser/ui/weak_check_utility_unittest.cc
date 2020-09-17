// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/weak_check_utility.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char kWeakShortPassword[] = "123456";
constexpr char kWeakLongPassword[] =
    "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcda";
constexpr char kStrongShortPassword[] = "fnlsr4@cm^mdls@fkspnsg3d";
constexpr char kStrongLongPassword[] =
    "pmsFlsnoab4nsl#losb@skpfnsbkjb^klsnbs!cns";

using ::testing::ElementsAre;

}  // namespace

TEST(WeakCheckUtilityTest, WeakPasswordsNotFound) {
  base::flat_set<base::string16> passwords = {
      base::ASCIIToUTF16(kStrongShortPassword),
      base::ASCIIToUTF16(kStrongLongPassword)};

  EXPECT_THAT(BulkWeakCheck(passwords), testing::IsEmpty());
}

TEST(WeakCheckUtilityTest, DetectedShortAndLongWeakPasswords) {
  base::flat_set<base::string16> passwords = {
      base::ASCIIToUTF16(kStrongLongPassword),
      base::ASCIIToUTF16(kWeakShortPassword),
      base::ASCIIToUTF16(kStrongShortPassword),
      base::ASCIIToUTF16(kWeakLongPassword)};

  base::flat_set<base::string16> weak_passwords = BulkWeakCheck(passwords);

  EXPECT_THAT(weak_passwords,
              ElementsAre(base::ASCIIToUTF16(kWeakShortPassword),
                          base::ASCIIToUTF16(kWeakLongPassword)));
}

}  // namespace password_manager
