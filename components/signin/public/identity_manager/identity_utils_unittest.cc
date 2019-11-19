// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_utils.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kUsername[] = "test@test.com";

const char kValidWildcardPattern[] = ".*@test.com";
const char kInvalidWildcardPattern[] = "*@test.com";

const char kMatchingPattern1[] = "test@test.com";
const char kMatchingPattern2[] = ".*@test.com";
const char kMatchingPattern3[] = "test@.*.com";
const char kMatchingPattern4[] = ".*@.*.com";
const char kMatchingPattern5[] = ".*@.*";
const char kMatchingPattern6[] = ".*";

const char kNonMatchingPattern[] = ".*foo.*";
const char kNonMatchingUsernamePattern[] = "foo@test.com";
const char kNonMatchingDomainPattern[] = "test@foo.com";
}  // namespace

class IdentityUtilsTest : public testing::Test {
 public:
  IdentityUtilsTest() {
    prefs_.registry()->RegisterStringPref(prefs::kGoogleServicesUsernamePattern,
                                          std::string());
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  TestingPrefServiceSimple prefs_;
};

TEST_F(IdentityUtilsTest, IsUsernameAllowedByPatternFromPrefs_EmptyPatterns) {
  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, "");
  EXPECT_TRUE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, "   ");
  EXPECT_FALSE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));
}

TEST_F(IdentityUtilsTest,
       IsUsernameAllowedByPatternFromPrefs_InvalidWildcardPatterns) {
  // signin::IsUsernameAllowedByPatternFromPrefs should recognize invalid
  // wildcard patterns like "*@foo.com" and insert a "." before them
  // automatically.
  prefs()->SetString(prefs::kGoogleServicesUsernamePattern,
                     kValidWildcardPattern);
  EXPECT_TRUE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern,
                     kInvalidWildcardPattern);
  EXPECT_TRUE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));
}

TEST_F(IdentityUtilsTest,
       IsUsernameAllowedByPatternFromPrefs_MatchingWildcardPatterns) {
  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern1);
  EXPECT_TRUE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern2);
  EXPECT_TRUE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern3);
  EXPECT_TRUE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern4);
  EXPECT_TRUE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern5);
  EXPECT_TRUE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern, kMatchingPattern6);
  EXPECT_TRUE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern,
                     kNonMatchingPattern);
  EXPECT_FALSE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern,
                     kNonMatchingUsernamePattern);
  EXPECT_FALSE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));

  prefs()->SetString(prefs::kGoogleServicesUsernamePattern,
                     kNonMatchingDomainPattern);
  EXPECT_FALSE(signin::IsUsernameAllowedByPatternFromPrefs(prefs(), kUsername));
}
