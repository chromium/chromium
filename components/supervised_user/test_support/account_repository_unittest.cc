// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/test_support/account_repository.h"

#include <optional>
#include <string>
#include <string_view>

#include "account_repository.h"
#include "base/files/file_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

base::FilePath WriteFile(std::string_view contents) {
  base::FilePath tmp_file;
  CHECK(base::CreateTemporaryFile(&tmp_file));
  CHECK(base::WriteFile(tmp_file, contents));
  return tmp_file;
}

TEST(AccountRepositoryTest, MinimalConfig) {
  std::string contents = R"json({})json";
  base::FilePath tmp_file = WriteFile(contents);
  TestAccountRepository repository(tmp_file);
  std::optional<test_accounts::Family> family =
      repository.GetRandomFamilyByFeature(test_accounts::Feature::REGULAR);
  ASSERT_FALSE(family.has_value());
}

TEST(AccountRepositoryTest, GetFamilyByFeature) {
  std::string contents = R"json(
  {
    "families": [
      {
        "feature": "DMA_INELIGIBLE",
        "members": [
          {
            "role": "PARENT",
            "username": "jan.nowak@gmail.com",
            "password": "password"
          }
        ]
      },
      {
        "feature": "REGULAR",
        "members": [
          {
            "role": "HEAD_OF_HOUSEHOLD",
            "username": "jan.kowalski@gmail.com",
            "password": "password"
          }
        ]
      }
    ]
  }
  )json";
  base::FilePath tmp_file = WriteFile(contents);
  TestAccountRepository repository(tmp_file);

  std::optional<test_accounts::Family> family =
      repository.GetRandomFamilyByFeature(test_accounts::Feature::REGULAR);
  ASSERT_TRUE(family.has_value());

  EXPECT_THAT(family->members, ::testing::SizeIs(1));
  EXPECT_EQ(family->members[0]->role,
            kidsmanagement::FamilyRole::HEAD_OF_HOUSEHOLD);
  EXPECT_EQ(family->members[0]->username, "jan.kowalski@gmail.com");
  EXPECT_EQ(family->members[0]->password, "password");
}

TEST(AccountRepositoryTest, GetFamilyMember) {
  std::string contents = R"json(
  {
    "families": [
      {
        "feature": "DMA_INELIGIBLE",
        "members": [
          {
            "role": "PARENT",
            "username": "jan.nowak@gmail.com",
            "password": "password"
          },
          {
            "role": "CHILD",
            "username": "monika.nowak@gmail.com",
            "password": "password"
          }
        ]
      },
      {
        "feature": "REGULAR",
        "members": [
          {
            "role": "HEAD_OF_HOUSEHOLD",
            "username": "jan.kowalski@gmail.com",
            "password": "password"
          }
        ]
      }
    ]
  }
  )json";
  base::FilePath tmp_file = WriteFile(contents);
  TestAccountRepository repository(tmp_file);

  std::optional<test_accounts::Family> family =
      repository.GetRandomFamilyByFeature(
          test_accounts::Feature::DMA_INELIGIBLE);
  ASSERT_TRUE(family.has_value());

  EXPECT_THAT(family->members, ::testing::SizeIs(2));

  std::optional<test_accounts::FamilyMember> child =
      GetFirstFamilyMemberByRole(*family, kidsmanagement::FamilyRole::CHILD);

  EXPECT_EQ(child->role, kidsmanagement::FamilyRole::CHILD);
  EXPECT_EQ(child->username, "monika.nowak@gmail.com");
  EXPECT_EQ(child->password, "password");
}

}  // namespace
}  // namespace supervised_user
