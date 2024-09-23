// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"

#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

using safe_browsing::SBThreatType;

namespace {
// Constants used in tests.
const SBThreatType kFirstThreatType =
    safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
const SBThreatType kSecondThreatType =
    safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
// Mocked SafeBrowsingUrlAllowList::Observer for use in tests.
class MockAllowListObserver : public SafeBrowsingUrlAllowList::Observer {
 public:
  MOCK_METHOD4(ThreatPolicyUpdated,
               void(SafeBrowsingUrlAllowList*,
                    const GURL&,
                    SBThreatType,
                    SafeBrowsingUrlAllowList::Policy));
  MOCK_METHOD4(ThreatPolicyBatchUpdated,
               void(SafeBrowsingUrlAllowList*,
                    const GURL&,
                    const std::set<SBThreatType>&,
                    SafeBrowsingUrlAllowList::Policy));
};
// Mocked WebStateObserver for use in tests.
class MockWebStateObserver : public web::WebStateObserver {
 public:
  MockWebStateObserver() {}
  ~MockWebStateObserver() override {}

  MOCK_METHOD1(DidChangeVisibleSecurityState, void(web::WebState*));
};
}

// Test fixture for SafeBrowsingUrlAllowList.
class SafeBrowsingUrlAllowListTest : public PlatformTest {
 public:
  SafeBrowsingUrlAllowListTest() {
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
    web_state_.AddObserver(&web_state_observer_);
    allow_list()->AddObserver(&allow_list_observer_);
  }
  ~SafeBrowsingUrlAllowListTest() override {
    web_state_.RemoveObserver(&web_state_observer_);
    allow_list()->RemoveObserver(&allow_list_observer_);
  }

  SafeBrowsingUrlAllowList* allow_list() {
    return SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  }

 protected:
  MockWebStateObserver web_state_observer_;
  MockAllowListObserver allow_list_observer_;
  web::FakeWebState web_state_;
};

// Tests that the allowed threat types are properly recorded.
TEST_F(SafeBrowsingUrlAllowListTest, AllowUnsafeNavigations) {
  const GURL url("http://www.chromium.test");

  // Unsafe navigations should not initially be allowed.
  EXPECT_FALSE(allow_list()->AreUnsafeNavigationsAllowed(url));

  // Allow navigations to |url| that encounter kFirstThreatType.
  EXPECT_CALL(allow_list_observer_,
              ThreatPolicyUpdated(allow_list(), url, kFirstThreatType,
                                  SafeBrowsingUrlAllowList::Policy::kAllowed));
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AllowUnsafeNavigations(url, kFirstThreatType);

  // Also allow navigations to |url| that encounter kSecondThreatType.
  EXPECT_CALL(allow_list_observer_,
              ThreatPolicyUpdated(allow_list(), url, kSecondThreatType,
                                  SafeBrowsingUrlAllowList::Policy::kAllowed));
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AllowUnsafeNavigations(url, kSecondThreatType);

  // Verify that navigations to |url| are allowed for both threat types.
  std::set<SBThreatType> allowed_threat_types;
  EXPECT_TRUE(
      allow_list()->AreUnsafeNavigationsAllowed(url, &allowed_threat_types));
  EXPECT_EQ(2U, allowed_threat_types.size());
  EXPECT_NE(allowed_threat_types.find(kFirstThreatType),
            allowed_threat_types.end());
  EXPECT_NE(allowed_threat_types.find(kSecondThreatType),
            allowed_threat_types.end());

  // Disallow unsafe navigations to |url| and verify that the list is updated.
  EXPECT_CALL(
      allow_list_observer_,
      ThreatPolicyBatchUpdated(allow_list(), url, allowed_threat_types,
                               SafeBrowsingUrlAllowList::Policy::kDisallowed));
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->DisallowUnsafeNavigations(url);
  EXPECT_FALSE(allow_list()->AreUnsafeNavigationsAllowed(url));
}

// Tests that pending unsafe navigation decisions are properly recorded.
TEST_F(SafeBrowsingUrlAllowListTest, AddPendingDecisions) {
  const GURL url("http://www.chromium.test");

  // The URL should not initially have any pending decisions.
  EXPECT_FALSE(allow_list()->IsUnsafeNavigationDecisionPending(url));

  // Add a pending decision for navigations to |url| that encounter
  // kFirstThreatType.
  EXPECT_CALL(allow_list_observer_,
              ThreatPolicyUpdated(allow_list(), url, kFirstThreatType,
                                  SafeBrowsingUrlAllowList::Policy::kPending));
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AddPendingUnsafeNavigationDecision(url, kFirstThreatType);

  // Add a pending decision for navigations to |url| that encounter
  // kSecondThreatType.
  EXPECT_CALL(allow_list_observer_,
              ThreatPolicyUpdated(allow_list(), url, kSecondThreatType,
                                  SafeBrowsingUrlAllowList::Policy::kPending));
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AddPendingUnsafeNavigationDecision(url, kSecondThreatType);

  // Verify that the pending decisions for both threat types are recorded.
  std::set<SBThreatType> pending_threat_types;
  EXPECT_TRUE(allow_list()->IsUnsafeNavigationDecisionPending(
      url, &pending_threat_types));
  EXPECT_EQ(2U, pending_threat_types.size());
  EXPECT_NE(pending_threat_types.find(kFirstThreatType),
            pending_threat_types.end());
  EXPECT_NE(pending_threat_types.find(kSecondThreatType),
            pending_threat_types.end());

  // Remove the pending decisions and verify that the allow list is updated.
  EXPECT_CALL(
      allow_list_observer_,
      ThreatPolicyBatchUpdated(allow_list(), url, pending_threat_types,
                               SafeBrowsingUrlAllowList::Policy::kDisallowed));
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->RemovePendingUnsafeNavigationDecisions(url);
  EXPECT_FALSE(allow_list()->IsUnsafeNavigationDecisionPending(url));
}

// Tests that the pending decisions for a threat type are erased if the threat
// has been allowed for that URL.
TEST_F(SafeBrowsingUrlAllowListTest, AllowPendingThreat) {
  const GURL url("http://www.chromium.test");
  EXPECT_CALL(allow_list_observer_,
              ThreatPolicyUpdated(allow_list(), url, kFirstThreatType,
                                  SafeBrowsingUrlAllowList::Policy::kPending));
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AddPendingUnsafeNavigationDecision(url, kFirstThreatType);

  // Allow |threat_type| and verify that the decision is no longer pending.
  EXPECT_CALL(allow_list_observer_,
              ThreatPolicyUpdated(allow_list(), url, kFirstThreatType,
                                  SafeBrowsingUrlAllowList::Policy::kAllowed));
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AllowUnsafeNavigations(url, kFirstThreatType);
  EXPECT_TRUE(allow_list()->AreUnsafeNavigationsAllowed(url));
  EXPECT_FALSE(allow_list()->IsUnsafeNavigationDecisionPending(url));
}

// Tests that allowed threats are recorded for the entire domain of a URL.
TEST_F(SafeBrowsingUrlAllowListTest, DomainAllowDecisions) {
  const GURL url("http://www.chromium.test");
  const GURL url_with_path("http://www.chromium.test/path");

  // Insert a pending decision and verify that it is pending for other URLs from
  // the same domain.
  EXPECT_CALL(allow_list_observer_,
              ThreatPolicyUpdated(allow_list(), url, kFirstThreatType,
                                  SafeBrowsingUrlAllowList::Policy::kPending));
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AddPendingUnsafeNavigationDecision(url, kFirstThreatType);
  EXPECT_TRUE(allow_list()->IsUnsafeNavigationDecisionPending(url_with_path));

  // Allowlist the URL and verify that it is allowed for other URLs from the
  // same domain.
  EXPECT_CALL(allow_list_observer_,
              ThreatPolicyUpdated(allow_list(), url, kFirstThreatType,
                                  SafeBrowsingUrlAllowList::Policy::kAllowed));
  EXPECT_CALL(web_state_observer_, DidChangeVisibleSecurityState(&web_state_));
  allow_list()->AllowUnsafeNavigations(url, kFirstThreatType);
  EXPECT_TRUE(allow_list()->AreUnsafeNavigationsAllowed(url_with_path));
}
