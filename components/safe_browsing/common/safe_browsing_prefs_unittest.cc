// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

class SafeBrowsingPrefsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingScoutReportingEnabled, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingSawInterstitialScoutReporting, false);
    prefs_.registry()->RegisterStringPref(
        prefs::kPasswordProtectionChangePasswordURL, "");
    prefs_.registry()->RegisterListPref(prefs::kPasswordProtectionLoginURLs);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
    prefs_.registry()->RegisterListPref(prefs::kSafeBrowsingWhitelistDomains);
  }

  void ResetPrefs(bool scout_reporting) {
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                      scout_reporting);
  }

  void ExpectPrefs(bool scout_reporting) {
    LOG(INFO) << "Pref values: scout=" << scout_reporting;
    EXPECT_EQ(scout_reporting,
              prefs_.GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled));
  }

  void ExpectPrefsExist(bool scout_reporting) {
    LOG(INFO) << "Prefs exist: scout=" << scout_reporting;
    EXPECT_EQ(scout_reporting,
              prefs_.HasPrefPath(prefs::kSafeBrowsingScoutReportingEnabled));
  }
  TestingPrefServiceSimple prefs_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

// TODO(crbug.com/881476) disabled for flaky crashes.
#if defined(OS_WIN)
#define MAYBE_GetSafeBrowsingExtendedReportingLevel \
  DISABLED_GetSafeBrowsingExtendedReportingLevel
#else
#define MAYBE_GetSafeBrowsingExtendedReportingLevel \
  GetSafeBrowsingExtendedReportingLevel
#endif
TEST_F(SafeBrowsingPrefsTest, MAYBE_GetSafeBrowsingExtendedReportingLevel) {
  // By Default, extended reporting is off.
  EXPECT_EQ(SBER_LEVEL_OFF, GetExtendedReportingLevel(prefs_));

  // The value of the Scout pref affects the reporting level directly.
  ResetPrefs(/*scout_reporting=*/true);
  EXPECT_EQ(SBER_LEVEL_SCOUT, GetExtendedReportingLevel(prefs_));
  // Scout pref off, so reporting is off.
  ResetPrefs(/*scout_reporting=*/false);
  EXPECT_EQ(SBER_LEVEL_OFF, GetExtendedReportingLevel(prefs_));
}

// TODO(crbug.com/881476) disabled for flaky crashes.
#if defined(OS_WIN)
#define MAYBE_VerifyMatchesPasswordProtectionLoginURL \
  DISABLED_VerifyMatchesPasswordProtectionLoginURL
#else
#define MAYBE_VerifyMatchesPasswordProtectionLoginURL \
  VerifyMatchesPasswordProtectionLoginURL
#endif
TEST_F(SafeBrowsingPrefsTest, MAYBE_VerifyMatchesPasswordProtectionLoginURL) {
  GURL url("https://mydomain.com/login.html#ref?username=alice");
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kPasswordProtectionLoginURLs));
  EXPECT_FALSE(MatchesPasswordProtectionLoginURL(url, prefs_));

  base::ListValue login_urls;
  login_urls.AppendString("https://otherdomain.com/login.html");
  prefs_.Set(prefs::kPasswordProtectionLoginURLs, login_urls);
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionLoginURLs));
  EXPECT_FALSE(MatchesPasswordProtectionLoginURL(url, prefs_));

  login_urls.AppendString("https://mydomain.com/login.html");
  prefs_.Set(prefs::kPasswordProtectionLoginURLs, login_urls);
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionLoginURLs));
  EXPECT_TRUE(MatchesPasswordProtectionLoginURL(url, prefs_));
}

TEST_F(SafeBrowsingPrefsTest,
       VerifyMatchesPasswordProtectionChangePasswordURL) {
  GURL url("https://mydomain.com/change_password.html#ref?username=alice");
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kPasswordProtectionChangePasswordURL));
  EXPECT_FALSE(MatchesPasswordProtectionChangePasswordURL(url, prefs_));

  prefs_.SetString(prefs::kPasswordProtectionChangePasswordURL,
                   "https://otherdomain.com/change_password.html");
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionChangePasswordURL));
  EXPECT_FALSE(MatchesPasswordProtectionChangePasswordURL(url, prefs_));

  prefs_.SetString(prefs::kPasswordProtectionChangePasswordURL,
                   "https://mydomain.com/change_password.html");
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionChangePasswordURL));
  EXPECT_TRUE(MatchesPasswordProtectionChangePasswordURL(url, prefs_));
}

TEST_F(SafeBrowsingPrefsTest, IsExtendedReportingPolicyManaged) {
  // This test checks that manipulating SBEROptInAllowed and the management
  // state of SBER behaves as expected. Below, we describe what should happen
  // to the results of IsExtendedReportingPolicyManaged and
  // IsExtendedReportingOptInAllowed.

  // Confirm default state, SBER should be disabled, OptInAllowed should
  // be enabled, and SBER is not managed.
  EXPECT_FALSE(IsExtendedReportingEnabled(prefs_));
  EXPECT_TRUE(IsExtendedReportingOptInAllowed(prefs_));
  EXPECT_FALSE(IsExtendedReportingPolicyManaged(prefs_));

  // Setting SBEROptInAllowed to false disallows opt-in but doesn't change
  // whether SBER is managed.
  prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);
  EXPECT_FALSE(IsExtendedReportingOptInAllowed(prefs_));
  EXPECT_FALSE(IsExtendedReportingPolicyManaged(prefs_));
  // Setting the value back to true reverts back to the default.
  prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
  EXPECT_TRUE(IsExtendedReportingOptInAllowed(prefs_));
  EXPECT_FALSE(IsExtendedReportingPolicyManaged(prefs_));

  // Make the SBER pref managed and enable it and ensure that the pref gets
  // the expected value. Making SBER managed doesn't change the
  // SBEROptInAllowed setting.
  prefs_.SetManagedPref(prefs::kSafeBrowsingScoutReportingEnabled,
                        std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs_.IsManagedPreference(prefs::kSafeBrowsingScoutReportingEnabled));
  // The value of the pref comes from the policy.
  EXPECT_TRUE(IsExtendedReportingEnabled(prefs_));
  // SBER being managed doesn't change the SBEROptInAllowed pref.
  EXPECT_TRUE(IsExtendedReportingOptInAllowed(prefs_));
}

TEST_F(SafeBrowsingPrefsTest, VerifyIsURLWhitelistedByPolicy) {
  GURL target_url("https://www.foo.com");
  // When PrefMember is null, URL is not whitelisted.
  EXPECT_FALSE(IsURLWhitelistedByPolicy(target_url, nullptr));

  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kSafeBrowsingWhitelistDomains));
  base::ListValue whitelisted_domains;
  whitelisted_domains.AppendString("foo.com");
  prefs_.Set(prefs::kSafeBrowsingWhitelistDomains, whitelisted_domains);
  StringListPrefMember string_list_pref;
  string_list_pref.Init(prefs::kSafeBrowsingWhitelistDomains, &prefs_);
  EXPECT_TRUE(IsURLWhitelistedByPolicy(target_url, prefs_));
  EXPECT_TRUE(IsURLWhitelistedByPolicy(target_url, &string_list_pref));

  GURL not_whitelisted_url("https://www.bar.com");
  EXPECT_FALSE(IsURLWhitelistedByPolicy(not_whitelisted_url, prefs_));
  EXPECT_FALSE(
      IsURLWhitelistedByPolicy(not_whitelisted_url, &string_list_pref));
}
}  // namespace safe_browsing
