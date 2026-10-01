// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/security_state/ios/security_state_utils.h"

#import <memory>

#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_state/core/security_state.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/security/security_style.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/cert/cert_status_flags.h"
#import "net/cert/x509_certificate.h"
#import "net/cert/x509_util.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

// This test fixture sets up a FakeWebState with a FakeNavigationManager and an
// initial HTTP NavigationItem.
class SecurityStateUtilsTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);

    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    url_ = GURL("http://chromium.test");
    std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
    item->SetURL(url_);
    item->GetSSL().security_style = web::SECURITY_STYLE_UNAUTHENTICATED;
    navigation_manager->SetVisibleItem(item.get());
    navigation_item_ = std::move(item);
    web_state_.SetNavigationManager(std::move(navigation_manager));
  }

  web::WebState* web_state() { return &web_state_; }

  GURL url_;
  std::unique_ptr<web::NavigationItem> navigation_item_;
  web::FakeWebState web_state_;
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
            SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE, SB_THREAT_TYPE_BILLING}},
          {security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_BLOCK,
           {SB_THREAT_TYPE_MANAGED_POLICY_BLOCK}},
          {security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_WARN,
           {SB_THREAT_TYPE_MANAGED_POLICY_WARN}}};

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

// Tests GetSecurityLevelForWebState() when an error page has a virtual URL with
// a certificate error.
TEST_F(SecurityStateUtilsTest,
       GetSecurityLevelForWebStateWithVirtualURLCertError) {
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetVisibleItem();
  ASSERT_TRUE(item);
  item->SetURL(GURL("file:///error_page.html"));
  item->SetVirtualURL(GURL("https://example.test"));
  item->GetSSL().security_style = web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  item->GetSSL().cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
  item->GetSSL().certificate = net::X509Certificate::CreateFromBytes(
      net::x509_util::CreateUnusableCert("CN=Error"));
  ASSERT_TRUE(item->GetSSL().certificate);

  EXPECT_EQ(security_state::DANGEROUS,
            security_state::GetSecurityLevelForWebState(web_state()));
}

// Tests GetMaliciousContentStatus() when an error page has a virtual URL with
// malicious content.
TEST_F(SecurityStateUtilsTest, GetMaliciousContentStatusWithVirtualURL) {
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetVisibleItem();
  ASSERT_TRUE(item);
  const GURL virtual_url("https://malware.test");
  item->SetURL(GURL("file:///error_page.html"));
  item->SetVirtualURL(virtual_url);

  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(web_state());
  allow_list->AddPendingUnsafeNavigationDecision(
      virtual_url, safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE);

  EXPECT_EQ(security_state::MALICIOUS_CONTENT_STATUS_MALWARE,
            security_state::GetMaliciousContentStatus(web_state()));
  EXPECT_EQ(security_state::DANGEROUS,
            security_state::GetSecurityLevelForWebState(web_state()));
}
