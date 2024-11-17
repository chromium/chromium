// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "account_repository.h"
#include "components/supervised_user/test_support/account_repository.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {
// TODO(crbug.com/373667523): Re-enable this test on ios.
#if BUILDFLAG(IS_IOS)
#define MAYBE_ProdConfigIsParseable DISABLED_ProdConfigIsParseable
#else
#define MAYBE_ProdConfigIsParseable ProdConfigIsParseable
#endif
TEST(AccountRepositoryInternalTest, MAYBE_ProdConfigIsParseable) {
  TestAccountRepository repository;
  std::optional<test_accounts::Family> family =
      repository.GetRandomFamilyByFeature(test_accounts::Feature::REGULAR);
  ASSERT_TRUE(family.has_value());
}
}  // namespace
}  // namespace supervised_user
