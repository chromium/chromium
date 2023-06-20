// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"

#include <memory>

#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace privacy_sandbox {

class PrivacySandboxAttestationsTestBase : public testing::Test {
 public:
  PrivacySandboxAttestationsTestBase()
      : scoped_attestations_(PrivacySandboxAttestations::CreateForTesting()) {}

 private:
  ScopedPrivacySandboxAttestations scoped_attestations_;
};

TEST_F(PrivacySandboxAttestationsTestBase, AddOverride) {
  net::SchemefulSite site(GURL("https://example.com"));
  ASSERT_FALSE(PrivacySandboxAttestations::GetInstance()->IsOverridden(site));

  PrivacySandboxAttestations::GetInstance()->AddOverride(site);
  EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()->IsOverridden(site));
}

TEST_F(PrivacySandboxAttestationsTestBase,
       SiteDefaultAttestedWithFeatureDefaultDisabled) {
  // Enrollment feature should be disabled by default.
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      privacy_sandbox::kEnforcePrivacySandboxAttestations));
  net::SchemefulSite site(GURL("https://example.com"));

  EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
      site, PrivacySandboxAttestationsGatedAPI::kTopics));
}

class PrivacySandboxAttestationsFeatureEnabledTest
    : public PrivacySandboxAttestationsTestBase {
 public:
  PrivacySandboxAttestationsFeatureEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        privacy_sandbox::kEnforcePrivacySandboxAttestations);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest,
       DefaultDenyIfAttestationsMapNotPresent) {
  net::SchemefulSite site(GURL("https://example.com"));

  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
      site, PrivacySandboxAttestationsGatedAPI::kTopics));
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest, AttestedIfOverridden) {
  net::SchemefulSite site(GURL("https://example.com"));
  ASSERT_FALSE(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
      site, PrivacySandboxAttestationsGatedAPI::kTopics));

  PrivacySandboxAttestations::GetInstance()->AddOverride(site);
  EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()->IsOverridden(site));
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest,
       EnrolledWithoutAttestations) {
  net::SchemefulSite site(GURL("https://example.com"));
  ASSERT_FALSE(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
      site, PrivacySandboxAttestationsGatedAPI::kTopics));

  PrivacySandboxAttestations::GetInstance()->SetAttestationsForTesting(
      {{site, {}}});

  EXPECT_FALSE(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
      site, PrivacySandboxAttestationsGatedAPI::kTopics));
}

TEST_F(PrivacySandboxAttestationsFeatureEnabledTest, EnrolledAndAttested) {
  net::SchemefulSite site(GURL("https://example.com"));
  ASSERT_FALSE(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
      site, PrivacySandboxAttestationsGatedAPI::kTopics));

  PrivacySandboxAttestations::GetInstance()->SetAttestationsForTesting(
      {{site, PrivacySandboxAttestationsGatedAPISet{
                  PrivacySandboxAttestationsGatedAPI::kTopics}}});

  EXPECT_TRUE(PrivacySandboxAttestations::GetInstance()->IsSiteAttested(
      site, PrivacySandboxAttestationsGatedAPI::kTopics));
}

}  // namespace privacy_sandbox
