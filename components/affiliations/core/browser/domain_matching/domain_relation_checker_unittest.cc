// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace affiliations {

namespace {

class DomainRelationCheckerTest : public testing::Test {
 public:
  DomainRelationCheckerTest() : checker_(fake_affiliation_service_) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  FakeAffiliationService fake_affiliation_service_;
  DomainRelationChecker checker_;
};

TEST_F(DomainRelationCheckerTest, ExactMatch) {
  base::test::TestFuture<std::optional<MatchType>> future;

  checker_.Check(url::Origin::Create(GURL("https://example.com")),
                 url::Origin::Create(GURL("https://example.com")),
                 future.GetCallback());

  EXPECT_EQ(future.Get(), MatchType::kExact);
}

TEST_F(DomainRelationCheckerTest, Mismatch_CacheMiss) {
  // Since example.com is not in cache, checking it will trigger cache miss.
  // Then UpdateAffiliationsAndBranding will cache it.
  // Since it is not affiliated with different.com, it will return mismatch.
  base::test::TestFuture<std::optional<MatchType>> future;

  checker_.Check(url::Origin::Create(GURL("https://example.com")),
                 url::Origin::Create(GURL("https://different.com")),
                 future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
}

TEST_F(DomainRelationCheckerTest, AffiliatedMatch_CacheMiss) {
  AffiliatedFacets group = {
      Facet(FacetURI::FromCanonicalSpec("https://example.com")),
      Facet(FacetURI::FromCanonicalSpec("https://affiliated.com"))};
  fake_affiliation_service_.AddAffiliationGroup(group, /*add_to_cache=*/false);
  base::test::TestFuture<std::optional<MatchType>> future;

  checker_.Check(url::Origin::Create(GURL("https://example.com")),
                 url::Origin::Create(GURL("https://affiliated.com")),
                 future.GetCallback());

  EXPECT_EQ(future.Get(), MatchType::kAffiliated);
}

TEST_F(DomainRelationCheckerTest, PSLMatch_CacheMiss) {
  base::test::TestFuture<std::optional<MatchType>> future;

  checker_.Check(url::Origin::Create(GURL("https://a.example.com")),
                 url::Origin::Create(GURL("https://b.example.com")),
                 future.GetCallback());

  EXPECT_EQ(future.Get(), MatchType::kPSL);
}

TEST_F(DomainRelationCheckerTest, Mismatch_PrivateRegistry) {
  // If private-registry.com is a PSL extension, then user1.private-registry.com
  // and user2.private-registry.com are not considered a PSL match.
  fake_affiliation_service_.SetPSLExtensions({"private-registry.com"});
  base::test::TestFuture<std::optional<MatchType>> future;

  checker_.Check(
      url::Origin::Create(GURL("https://user1.private-registry.com")),
      url::Origin::Create(GURL("https://user2.private-registry.com")),
      future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
}

TEST_F(DomainRelationCheckerTest, GroupedMatch_CacheMiss) {
  GroupedFacets group;
  group.facets.push_back(
      Facet(FacetURI::FromCanonicalSpec("https://example.com")));
  group.facets.push_back(
      Facet(FacetURI::FromCanonicalSpec("https://grouped.com")));
  fake_affiliation_service_.AddGroupedFacets(group, /*add_to_cache=*/false);
  base::test::TestFuture<std::optional<MatchType>> future;

  checker_.Check(url::Origin::Create(GURL("https://example.com")),
                 url::Origin::Create(GURL("https://grouped.com")),
                 future.GetCallback());

  EXPECT_EQ(future.Get(), MatchType::kGrouped);
}

TEST_F(DomainRelationCheckerTest, MixedMatch_CacheMiss) {
  // Mixed match tests when both Affiliation and PSL match are present for
  // a.example.com and b.example.com.
  // They are a PSL match because they share example.com.
  // They are also seeded as affiliated.
  AffiliatedFacets group = {
      Facet(FacetURI::FromCanonicalSpec("https://a.example.com")),
      Facet(FacetURI::FromCanonicalSpec("https://b.example.com"))};
  fake_affiliation_service_.AddAffiliationGroup(group, /*add_to_cache=*/false);
  base::test::TestFuture<std::optional<MatchType>> future;

  checker_.Check(url::Origin::Create(GURL("https://a.example.com")),
                 url::Origin::Create(GURL("https://b.example.com")),
                 future.GetCallback());

  EXPECT_EQ(future.Get(), MatchType::kAffiliated | MatchType::kPSL);
}

TEST_F(DomainRelationCheckerTest, AffiliatedMatch_CacheHit) {
  AffiliatedFacets group = {
      Facet(FacetURI::FromCanonicalSpec("https://example.com")),
      Facet(FacetURI::FromCanonicalSpec("https://affiliated.com"))};
  // By default, add_to_cache=true.
  fake_affiliation_service_.AddAffiliationGroup(group);
  base::test::TestFuture<std::optional<MatchType>> future;

  checker_.Check(url::Origin::Create(GURL("https://example.com")),
                 url::Origin::Create(GURL("https://affiliated.com")),
                 future.GetCallback());

  EXPECT_EQ(future.Get(), MatchType::kAffiliated);
}

}  // namespace
}  // namespace affiliations
