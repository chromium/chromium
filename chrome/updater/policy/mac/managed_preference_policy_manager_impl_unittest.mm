// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/policy/mac/managed_preference_policy_manager_impl.h"

#include "base/enterprise_util.h"
#include "chrome/updater/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace updater {

TEST(CRUManagedPreferencePolicyManagerTest, TestPolicyValues) {
  CRUUpdatePolicyDictionary* policyDict = @{
    @"global" : @{
      @"UpdateDefault" : @3,  // Update disabled
      @"DownloadPreference" : @"cacheable",
      @"UpdatesSuppressedStartHour" : @12.345,
      @"UpdatesSuppressedStartMin" : @"15",
      @"UpdatesSuppressedDurationMin" : @30,
    },
    @"com.google.Keystone" : @{
      @"UpdateDefault" : @3,  // Update disabled
    },
    @"com.google.Chrome" : @{
      @"UpdateDefault" : @2,  // Manual update only
      @"RollbackToTargetVersion" : @1,
      @"TargetVersionPrefix" : @"82.",
    },
  };
  CRUManagedPreferencePolicyManager* policyManager =
      [[CRUManagedPreferencePolicyManager alloc] initWithDictionary:policyDict];
  EXPECT_NSEQ([policyManager source], @"Managed Preferences");
  EXPECT_TRUE(policyManager.hasActivePolicy);

  // Verify global level policies.
  EXPECT_EQ(policyManager.lastCheckPeriodMinutes, kPolicyNotSet);
  EXPECT_EQ(policyManager.defaultUpdatePolicy, kPolicyDisabled);
  EXPECT_NSEQ(policyManager.downloadPreference, @"cacheable");
  EXPECT_EQ(policyManager.updatesSuppressed.start_hour_, 12);
  EXPECT_EQ(policyManager.updatesSuppressed.start_minute_, 15);
  EXPECT_EQ(policyManager.updatesSuppressed.duration_minute_, 30);
  EXPECT_EQ(policyManager.proxyMode, nil);
  EXPECT_EQ(policyManager.proxyServer, nil);
  EXPECT_EQ(policyManager.proxyPacURL, nil);

  // App-level policies.
  NSString* keystoneID = @"com.google.Keystone";
  EXPECT_EQ([policyManager appUpdatePolicy:keystoneID], kPolicyDisabled);
  EXPECT_EQ([policyManager rollbackToTargetVersion:keystoneID], kPolicyNotSet);
  EXPECT_EQ([policyManager targetVersionPrefix:keystoneID], nil);

  NSString* chromeID = @"com.google.Chrome";
  EXPECT_EQ([policyManager appUpdatePolicy:chromeID], kPolicyManualUpdatesOnly);
  EXPECT_EQ([policyManager rollbackToTargetVersion:chromeID], 1);
  EXPECT_NSEQ([policyManager targetVersionPrefix:chromeID], @"82.");

  NSString* nonExistApp = @"com.google.NonexistProduct";
  EXPECT_EQ([policyManager appUpdatePolicy:nonExistApp], kPolicyNotSet);
  EXPECT_EQ([policyManager rollbackToTargetVersion:nonExistApp], kPolicyNotSet);
  EXPECT_EQ([policyManager targetVersionPrefix:nonExistApp], nil);
}

TEST(CRUManagedPreferencePolicyManagerTest, TestEmptyPolicyValues) {
  CRUUpdatePolicyDictionary* policyDict = @{};
  CRUManagedPreferencePolicyManager* policyManager =
      [[CRUManagedPreferencePolicyManager alloc] initWithDictionary:policyDict];
  EXPECT_FALSE(policyManager.hasActivePolicy);
}

TEST(CRUManagedPreferencePolicyManagerTest, TestNoGlobalPolicy) {
  CRUUpdatePolicyDictionary* policyDict = @{@"some.app" : @{}};
  CRUManagedPreferencePolicyManager* policyManager =
      [[CRUManagedPreferencePolicyManager alloc] initWithDictionary:policyDict];
  EXPECT_NSEQ(policyManager.source, @"Managed Preferences");
  EXPECT_TRUE(policyManager.hasActivePolicy);

  // Verify global level policies are set to default.
  EXPECT_EQ(policyManager.lastCheckPeriodMinutes, kPolicyNotSet);
  EXPECT_EQ(policyManager.defaultUpdatePolicy, kPolicyNotSet);
  EXPECT_NSEQ(policyManager.downloadPreference, nil);
  EXPECT_EQ(policyManager.updatesSuppressed.start_hour_, kPolicyNotSet);
  EXPECT_EQ(policyManager.updatesSuppressed.start_minute_, kPolicyNotSet);
  EXPECT_EQ(policyManager.updatesSuppressed.duration_minute_, kPolicyNotSet);
  EXPECT_EQ(policyManager.proxyMode, nil);
  EXPECT_EQ(policyManager.proxyServer, nil);
  EXPECT_EQ(policyManager.proxyPacURL, nil);
}

TEST(CRUManagedPreferencePolicyManagerTest, TestInvalidPolicyValues) {
  // Verify that unexpected policy settings are ignored.
  NSDictionary* policyDict = @{
    @"global" : @{
      @"UpdateDefault" : @"ExpectNumber",
      @"DownloadPreference" : @1,
      @"UpdatesSuppressedStartHour" : @[ @1 ],
      @"UpdatesSuppressedStartMin" : @[],
      @"UpdatesSuppressedDurationMin" : @"Foo",
    },
    @"com.google.Keystone" : @{
      @"UnknownKey" : @3,
      @2 : @"KeyIsNotString",
    },
    @"com.google.Chrome" : @{
      @"UpdateDefault" : @"WrongValue",
      @"RollbackToTargetVersion" : @{},
      @"TargetVersionPrefix" : @{},
    },
    @"com.google.Foo" : @"PolicyValueIsNotDictionary",
  };
  CRUManagedPreferencePolicyManager* policyManager =
      [[CRUManagedPreferencePolicyManager alloc] initWithDictionary:policyDict];
  EXPECT_NSEQ(policyManager.source, @"Managed Preferences");
  EXPECT_TRUE(policyManager.hasActivePolicy);

  // Verify global level policies.
  EXPECT_EQ(policyManager.lastCheckPeriodMinutes, kPolicyNotSet);
  EXPECT_EQ(policyManager.defaultUpdatePolicy, kPolicyNotSet);
  EXPECT_EQ(policyManager.downloadPreference, nil);
  EXPECT_EQ(policyManager.updatesSuppressed.start_hour_, kPolicyNotSet);
  EXPECT_EQ(policyManager.updatesSuppressed.start_minute_, kPolicyNotSet);
  EXPECT_EQ(policyManager.updatesSuppressed.duration_minute_, kPolicyNotSet);
  EXPECT_EQ(policyManager.proxyMode, nil);
  EXPECT_EQ(policyManager.proxyServer, nil);
  EXPECT_EQ(policyManager.proxyPacURL, nil);

  // App-level policies.
  NSString* keystoneID = @"com.google.Keystone";
  EXPECT_EQ([policyManager appUpdatePolicy:keystoneID], kPolicyNotSet);
  EXPECT_EQ([policyManager rollbackToTargetVersion:keystoneID], kPolicyNotSet);
  EXPECT_EQ([policyManager targetVersionPrefix:keystoneID], nil);

  NSString* chromeID = @"com.google.Chrome";
  EXPECT_EQ([policyManager appUpdatePolicy:chromeID], kPolicyNotSet);
  EXPECT_EQ([policyManager rollbackToTargetVersion:chromeID], kPolicyNotSet);
  EXPECT_EQ([policyManager targetVersionPrefix:chromeID], nil);

  EXPECT_EQ([policyManager appUpdatePolicy:@"com.google.Foo"], kPolicyNotSet);
}

}  // namespace updater
