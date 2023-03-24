// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/include_exclude_account_id_filter.h"

#include <string>
#include <vector>

#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_manager {

namespace {

constexpr char kIncludedItem1[] = "included_item1";
constexpr char kIncludedItem2[] = "included_item2";

constexpr char kExcludedItem1[] = "excluded_item1";
constexpr char kExcludedItem2[] = "excluded_item2";

constexpr char kNonListedItem[] = "non_listed_item";

IncludeExcludeAccountIdFilter CreateIncludeExcludeAccountIdFilter(
    bool included_by_default) {
  return IncludeExcludeAccountIdFilter(
      included_by_default,
      {AccountId::FromUserEmail(kIncludedItem1),
       AccountId::FromUserEmail(kIncludedItem2)},
      {AccountId::FromUserEmail(kExcludedItem1),
       AccountId::FromUserEmail(kExcludedItem2)});
}

}  // namespace

TEST(IncludeExcludeAccountIdFilterTest, DefaultConstructor) {
  const IncludeExcludeAccountIdFilter filter;
  EXPECT_FALSE(
      filter.IsAccountIdIncluded(AccountId::FromUserEmail(kNonListedItem)));
}

TEST(IncludeExcludeAccountIdFilterTest, IncludedByDefault) {
  const auto filter = CreateIncludeExcludeAccountIdFilter(true);
  EXPECT_TRUE(
      filter.IsAccountIdIncluded(AccountId::FromUserEmail(kNonListedItem)));

  EXPECT_TRUE(
      filter.IsAccountIdIncluded(AccountId::FromUserEmail(kIncludedItem1)));
  EXPECT_TRUE(
      filter.IsAccountIdIncluded(AccountId::FromUserEmail(kIncludedItem2)));

  EXPECT_FALSE(
      filter.IsAccountIdIncluded(AccountId::FromUserEmail(kExcludedItem1)));
  EXPECT_FALSE(
      filter.IsAccountIdIncluded(AccountId::FromUserEmail(kExcludedItem2)));
}

TEST(IncludeExcludeAccountIdFilterTest, ExcludedByDefault) {
  const auto filter = CreateIncludeExcludeAccountIdFilter(false);
  EXPECT_FALSE(
      filter.IsAccountIdIncluded(AccountId::FromUserEmail(kNonListedItem)));

  EXPECT_TRUE(
      filter.IsAccountIdIncluded(AccountId::FromUserEmail(kIncludedItem1)));
  EXPECT_TRUE(
      filter.IsAccountIdIncluded(AccountId::FromUserEmail(kIncludedItem2)));

  EXPECT_FALSE(
      filter.IsAccountIdIncluded(AccountId::FromUserEmail(kExcludedItem1)));
  EXPECT_FALSE(
      filter.IsAccountIdIncluded(AccountId::FromUserEmail(kExcludedItem2)));
}

}  // namespace user_manager
