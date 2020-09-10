// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/weak_check_utility.h"

#include <vector>
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

constexpr char kUsername1[] = "alice";
constexpr char kUsername2[] = "bob";

constexpr char kWeakShortPassword[] = "123456";
constexpr char kWeakLongPassword[] =
    "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcda";
constexpr char kStrongShortPassword[] = "fnlsr4@cm^mdls@fkspnsg3d";
constexpr char kStrongLongPassword[] =
    "pmsFlsnoab4nsl#losb@skpfnsbkjb^klsnbs!cns";

using autofill::PasswordForm;
using ::testing::ElementsAre;

PasswordForm MakeSavedPassword(base::StringPiece username,
                               base::StringPiece password) {
  PasswordForm form;
  form.signon_realm = "https://example.com";
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  return form;
}

}  // namespace

TEST(WeakCheckUtilityTest, WeakPasswordsNotFound) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kUsername1, kStrongShortPassword),
      MakeSavedPassword(kUsername2, kStrongLongPassword)};

  EXPECT_THAT(BulkWeakCheck(passwords), testing::IsEmpty());
}

TEST(WeakCheckUtilityTest, DetectedShortAndLongWeakPasswords) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kUsername1, kStrongLongPassword),
      MakeSavedPassword(kUsername1, kWeakShortPassword),
      MakeSavedPassword(kUsername1, kStrongShortPassword),
      MakeSavedPassword(kUsername2, kWeakLongPassword)};

  base::flat_set<base::string16> weak_passwords = BulkWeakCheck(passwords);

  EXPECT_THAT(weak_passwords,
              ElementsAre(base::ASCIIToUTF16(kWeakShortPassword),
                          base::ASCIIToUTF16(kWeakLongPassword)));
}

TEST(WeakCheckUtilityTest, WeakPasswordsSetContainsNoCopies) {
  std::vector<PasswordForm> passwords = {
      MakeSavedPassword(kUsername1, kWeakShortPassword),
      MakeSavedPassword(kUsername2, kWeakShortPassword)};

  base::flat_set<base::string16> weak_passwords = BulkWeakCheck(passwords);

  EXPECT_THAT(weak_passwords,
              ElementsAre(base::ASCIIToUTF16(kWeakShortPassword)));
}

}  // namespace password_manager
