// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/ios/security_state_utils.h"

#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#include "components/security_state/core/security_state.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"

// This test fixture creates an IOSSecurityStateTabHelper, then loads a
// non-secure HTML document.
class SecurityStateUtilsTest : public web::WebTestWithWebState {
 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    SafeBrowsingUrlAllowList::CreateForWebState(web_state());

    url_ = GURL("http://chromium.test");
    LoadHtml(@"<html><body></body></html>", url_);
  }

  GURL url_;
};

// Verifies GetMaliciousContentStatus() return values.
TEST_F(SecurityStateUtilsTest, GetMaliciousContentStatus) {
  using enum safe_browsing::SBThreatType;

  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(web_state());
  std::map<security_state::MaliciousContentStatus,
           std::vector<safe_browsing::SBThreatType>>
      threats_types_for_content_statuses = {
          {security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
           {SB_THREAT_TYPE_UNUSED, SB_THREAT_TYPE_SAFE,
            SB_THREAT_TYPE_URL_PHISHING,
            SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING}},
          {security_state::MALICIOUS_CONTENT_STATUS_MALWARE,
           {SB_THREAT_TYPE_URL_MALWARE}},
          {security_state::MALICIOUS_CONTENT_STATUS_UNWANTED_SOFTWARE,
           {SB_THREAT_TYPE_URL_UNWANTED}},
          {security_state::MALICIOUS_CONTENT_STATUS_BILLING,
           {SB_THREAT_TYPE_SAVED_PASSWORD_REUSE,
            SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE,
            SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE,
            SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE, SB_THREAT_TYPE_BILLING}}};

  for (auto& pair : threats_types_for_content_statuses) {
    security_state::MaliciousContentStatus status = pair.first;
    for (auto& threat : pair.second) {
      allow_list->RemovePendingUnsafeNavigationDecisions(url_);
      allow_list->AddPendingUnsafeNavigationDecision(url_, threat);
      EXPECT_EQ(status, security_state::GetMaliciousContentStatus(web_state()))
          << "Unexpected MaliciousContentStatus for SBThreatType: "
          << static_cast<int>(threat);
    }
  }
}

// Verifies GetSecurityLevelForWebState() for malicious content.
TEST_F(SecurityStateUtilsTest, GetSecurityLevelForWebStateMaliciousContent) {
  // The test fixture loads an http page, so the initial state is WARNING.
  ASSERT_EQ(security_state::WARNING,
            security_state::GetSecurityLevelForWebState(web_state()));

  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(web_state());
  allow_list->AddPendingUnsafeNavigationDecision(
      url_, safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  EXPECT_EQ(security_state::DANGEROUS,
            security_state::GetSecurityLevelForWebState(web_state()));
}
