// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/gmail_otp_sender_domain_matcher.h"

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace one_time_tokens {

namespace {

using ::affiliations::AffiliatedFacets;
using ::affiliations::Facet;
using ::affiliations::FacetURI;

class GmailOtpSenderDomainMatcherTest : public testing::Test {
 public:
  affiliations::FakeAffiliationService& affiliation_service() {
    return affiliation_service_;
  }

  // Runs a single check to completion and returns the resolved match type.
  GmailOtpSenderDomainMatchType CheckSender(std::string_view sender_address,
                                            std::string_view frame_url) {
    GmailOtpSenderDomainMatcher matcher(domain_relation_checker(),
                                        url::Origin::Create(GURL(frame_url)));
    base::test::TestFuture<GmailOtpSenderDomainMatchType> future;
    matcher.Check(sender_address, future.GetCallback());
    return future.Take();
  }

  std::unique_ptr<affiliations::DomainRelationChecker>
  domain_relation_checker() {
    return std::make_unique<affiliations::DomainRelationChecker>(
        affiliation_service_);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  affiliations::FakeAffiliationService affiliation_service_;
};

TEST_F(GmailOtpSenderDomainMatcherTest, SameDomainIsExact) {
  EXPECT_EQ(CheckSender("no-reply@example.com", "https://example.com"),
            GmailOtpSenderDomainMatchType::kExact);
}

TEST_F(GmailOtpSenderDomainMatcherTest, WwwFrameMatchingSenderIsFrameIsWwwPsl) {
  // The frame origin has www., the sender domain does not. This is a PSL match
  // which resolves to the same host once www. is stripped from the frame.
  EXPECT_EQ(CheckSender("no-reply@example.com", "https://www.example.com"),
            GmailOtpSenderDomainMatchType::kFrameIsWwwPsl);
}

TEST_F(GmailOtpSenderDomainMatcherTest,
       WwwSubdomainFrameMatchingSubdomainSenderIsFrameIsWwwPsl) {
  // The frame origin is www.sub.example.com and the sender is sub.example.com:
  // a PSL match which resolves to the same host once www. is stripped.
  EXPECT_EQ(
      CheckSender("no-reply@sub.example.com", "https://www.sub.example.com"),
      GmailOtpSenderDomainMatchType::kFrameIsWwwPsl);
}

TEST_F(GmailOtpSenderDomainMatcherTest, WwwSubdomainFrameAndRootSenderArePsl) {
  // The frame origin is www.sub.example.com and the sender is example.com:
  // stripping www. yields sub.example.com, which is not the sender, so this
  // stays a plain PSL match.
  EXPECT_EQ(CheckSender("no-reply@example.com", "https://www.sub.example.com"),
            GmailOtpSenderDomainMatchType::kPsl);
}

TEST_F(GmailOtpSenderDomainMatcherTest, WwwSenderIsPsl) {
  // The sender domain has www., the frame origin does not, so this is only a
  // PSL match.
  EXPECT_EQ(CheckSender("no-reply@www.example.com", "https://example.com"),
            GmailOtpSenderDomainMatchType::kPsl);
}

TEST_F(GmailOtpSenderDomainMatcherTest, WwwSenderAndWwwFrameAreExact) {
  EXPECT_EQ(CheckSender("no-reply@www.example.com", "https://www.example.com"),
            GmailOtpSenderDomainMatchType::kExact);
}

TEST_F(GmailOtpSenderDomainMatcherTest, AffiliatedSenderIsAffiliated) {
  affiliation_service().AddAffiliationGroup(AffiliatedFacets{
      {Facet{FacetURI::FromCanonicalSpec("https://example.com")},
       Facet{FacetURI::FromCanonicalSpec("https://affiliated.com")}}});

  EXPECT_EQ(CheckSender("auth@affiliated.com", "https://example.com"),
            GmailOtpSenderDomainMatchType::kAffiliated);
}

TEST_F(GmailOtpSenderDomainMatcherTest, SubdomainSenderIsPsl) {
  EXPECT_EQ(CheckSender("service@sub.example.com", "https://example.com"),
            GmailOtpSenderDomainMatchType::kPsl);
}

TEST_F(GmailOtpSenderDomainMatcherTest, GroupedSenderIsGrouped) {
  affiliations::GroupedFacets group;
  group.facets.emplace_back(FacetURI::FromCanonicalSpec("https://example.com"));
  group.facets.emplace_back(FacetURI::FromCanonicalSpec("https://grouped.com"));
  affiliation_service().AddGroupedFacets(group);

  EXPECT_EQ(CheckSender("sender@grouped.com", "https://example.com"),
            GmailOtpSenderDomainMatchType::kGrouped);
}

TEST_F(GmailOtpSenderDomainMatcherTest, UnrelatedSenderHasNoMatch) {
  EXPECT_EQ(CheckSender("sender@nomatch.com", "https://example.com"),
            GmailOtpSenderDomainMatchType::kNoMatch);
}

TEST_F(GmailOtpSenderDomainMatcherTest, SenderWithoutDomainHasNoMatch) {
  EXPECT_EQ(CheckSender("not-an-email-address", "https://example.com"),
            GmailOtpSenderDomainMatchType::kNoMatch);
}

TEST_F(GmailOtpSenderDomainMatcherTest, EmptySenderHasNoMatch) {
  EXPECT_EQ(CheckSender("", "https://example.com"),
            GmailOtpSenderDomainMatchType::kNoMatch);
}

TEST_F(GmailOtpSenderDomainMatcherTest, OneMatcherAnswersConcurrentChecks) {
  GmailOtpSenderDomainMatcher matcher(
      domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")));

  base::test::TestFuture<GmailOtpSenderDomainMatchType> exact;
  base::test::TestFuture<GmailOtpSenderDomainMatchType> no_match;
  matcher.Check("no-reply@example.com", exact.GetCallback());
  matcher.Check("sender@nomatch.com", no_match.GetCallback());

  EXPECT_EQ(exact.Get(), GmailOtpSenderDomainMatchType::kExact);
  EXPECT_EQ(no_match.Get(), GmailOtpSenderDomainMatchType::kNoMatch);
}

TEST_F(GmailOtpSenderDomainMatcherTest, DestructionCancelsTheCheck) {
  auto matcher = std::make_unique<GmailOtpSenderDomainMatcher>(
      domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")));

  bool callback_ran = false;
  matcher->Check("no-reply@example.com",
                 base::BindOnce(
                     [](bool* callback_ran, GmailOtpSenderDomainMatchType) {
                       *callback_ran = true;
                     },
                     &callback_ran));

  // Destroying the matcher also destroys the `DomainRelationChecker` it owns.
  matcher.reset();

  // Run an identical check to completion. It takes the same path through the
  // affiliation service, so the cancelled check would have delivered its
  // result by the time this one does.
  GmailOtpSenderDomainMatcher sentinel(
      domain_relation_checker(),
      url::Origin::Create(GURL("https://example.com")));
  base::test::TestFuture<GmailOtpSenderDomainMatchType> future;
  sentinel.Check("no-reply@example.com", future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_FALSE(callback_ran);
}

}  // namespace
}  // namespace one_time_tokens
