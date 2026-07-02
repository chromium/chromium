// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace affiliations {

namespace {

class DomainRelationCheckerTest : public testing::Test {
 public:
  DomainRelationCheckerTest() : checker_(mock_affiliation_service_) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::StrictMock<MockAffiliationService> mock_affiliation_service_;
  DomainRelationChecker checker_;
};

TEST_F(DomainRelationCheckerTest, ExactMatch) {
  base::test::TestFuture<std::optional<MatchType>> future;
  checker_.Check(url::Origin::Create(GURL("https://example.com")),
                 url::Origin::Create(GURL("https://example.com")),
                 future.GetCallback());
  EXPECT_EQ(future.Get(), MatchType::kExact);
}

TEST_F(DomainRelationCheckerTest, GURLOverload) {
  base::test::TestFuture<std::optional<MatchType>> future;
  checker_.Check(GURL("https://example.com/path1"),
                 GURL("https://example.com/path2"), future.GetCallback());
  EXPECT_EQ(future.Get(), MatchType::kExact);
}

TEST_F(DomainRelationCheckerTest, Mismatch) {
  base::test::TestFuture<std::optional<MatchType>> future;
  checker_.Check(url::Origin::Create(GURL("https://example.com")),
                 url::Origin::Create(GURL("https://different.com")),
                 future.GetCallback());
  EXPECT_EQ(future.Get(), std::nullopt);
}

}  // namespace
}  // namespace affiliations
