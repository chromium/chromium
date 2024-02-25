// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previous_session_info/previous_session_info.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/time/time.h"
#include "components/previous_session_info/previous_session_info_private.h"
#include "components/version_info/version_info.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

using previous_session_info_constants::
    kPreviousSessionInfoConnectedSceneSessionIDs;
using previous_session_info_constants::kPreviousSessionInfoInactiveTabCount;
using previous_session_info_constants::kPreviousSessionInfoMemoryFootprint;
using previous_session_info_constants::kPreviousSessionInfoOTRTabCount;
using previous_session_info_constants::kPreviousSessionInfoParamsPrefix;
using previous_session_info_constants::kPreviousSessionInfoRestoringSession;
using previous_session_info_constants::kPreviousSessionInfoTabCount;

namespace {

const NSInteger kTabCount = 15;
const NSInteger kInactiveTabCount = 30;

// Key in the UserDefaults for a boolean value keeping track of memory warnings.
NSString* const kDidSeeMemoryWarningShortlyBeforeTerminating =
    previous_session_info_constants::
        kDidSeeMemoryWarningShortlyBeforeTerminating;

// Key in the NSUserDefaults for a string value that stores the version of the
// last session.
NSString* const kLastRanVersion = @"LastRanVersion";
// Key in the NSUserDefaults for a string value that stores the language of the
// last session.
NSString* const kLastRanLanguage = @"LastRanLanguage";

// IDs to be used for testing scene sessions.
NSString* const kTestSession1ID = @"test_session_1";
NSString* const kTestSession2ID = @"test_session_2";
NSString* const kTestSession3ID = @"test_session_3";

NSDictionary* GetParamsDictionary() {
  NSMutableDictionary* reportParameters = [[NSMutableDictionary alloc] init];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSUInteger prefix_length = kPreviousSessionInfoParamsPrefix.length;
  for (NSString* key in [defaults dictionaryRepresentation].allKeys) {
    if ([key hasPrefix:kPreviousSessionInfoParamsPrefix]) {
      NSString* crash_key = [key substringFromIndex:prefix_length];
      reportParameters[crash_key] = [defaults stringForKey:key];
    }
  }
  return reportParameters;
}

using PreviousSessionInfoTest = PlatformTest;

TEST_F(PreviousSessionInfoTest, InitializationWithEmptyDefaults) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];
  [defaults removeObjectForKey:kLastRanLanguage];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the default values.
  EXPECT_FALSE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_TRUE([sharedInstance isFirstSessionAfterUpgrade]);
  EXPECT_TRUE([sharedInstance isFirstSessionAfterLanguageChange]);
}

TEST_F(PreviousSessionInfoTest, InitializationWithSameLanguage) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kLastRanLanguage];

  // Set the current language as the last ran language.
  NSString* currentVersion = [[NSLocale preferredLanguages] objectAtIndex:0];
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_TRUE([sharedInstance isFirstSessionAfterLanguageChange]);
}

TEST_F(PreviousSessionInfoTest, InitializationWithDifferentLanguage) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kLastRanLanguage];

  // Set the current language as the last ran language.
  NSString* currentVersion = @"Fake Language";
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_TRUE([sharedInstance isFirstSessionAfterLanguageChange]);
}

TEST_F(PreviousSessionInfoTest, InitializationWithSameVersionNoMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  NSString* currentVersion =
      base::SysUTF8ToNSString(version_info::GetVersionNumber());
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_FALSE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_FALSE([sharedInstance isFirstSessionAfterUpgrade]);
}

TEST_F(PreviousSessionInfoTest, InitializationWithSameVersionMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  NSString* currentVersion =
      base::SysUTF8ToNSString(version_info::GetVersionNumber());
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_TRUE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_FALSE([sharedInstance isFirstSessionAfterUpgrade]);
}

TEST_F(PreviousSessionInfoTest, InitializationDifferentVersionNoMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  [defaults setObject:@"Fake Version" forKey:kLastRanVersion];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_FALSE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_TRUE([sharedInstance isFirstSessionAfterUpgrade]);
}

TEST_F(PreviousSessionInfoTest, InitializationDifferentVersionMemoryWarning) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the current version as the last ran version.
  [defaults setObject:@"Fake Version" forKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Instantiate the PreviousSessionInfo sharedInstance.
  PreviousSessionInfo* sharedInstance = [PreviousSessionInfo sharedInstance];

  // Checks the values.
  EXPECT_TRUE([sharedInstance didSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_TRUE([sharedInstance isFirstSessionAfterUpgrade]);
}

// Creates conditions that exist on the first app run and tests
// OSRestartedAfterPreviousSession property.
TEST_F(PreviousSessionInfoTest, InitializationWithoutSystemStartTime) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:previous_session_info_constants::kOSStartTime];

  EXPECT_FALSE(
      [[PreviousSessionInfo sharedInstance] OSRestartedAfterPreviousSession]);
}

// Creates conditions that exist when OS was restarted after the previous app
// run and tests OSRestartedAfterPreviousSession property.
TEST_F(PreviousSessionInfoTest, InitializationAfterOSRestart) {
  [PreviousSessionInfo resetSharedInstanceForTesting];

  // For the previous session OS started 60 seconds before OS has started for
  // this session.
  NSTimeInterval current_system_start_time =
      NSDate.timeIntervalSinceReferenceDate -
      NSProcessInfo.processInfo.systemUptime;
  [[NSUserDefaults standardUserDefaults]
      setDouble:current_system_start_time - 60
         forKey:previous_session_info_constants::kOSStartTime];

  EXPECT_TRUE(
      [[PreviousSessionInfo sharedInstance] OSRestartedAfterPreviousSession]);
}

// Creates conditions that exist when OS was not restarted after the previous
// app run and tests OSRestartedAfterPreviousSession property.
TEST_F(PreviousSessionInfoTest, InitializationForSecondSessionAfterOSRestart) {
  [PreviousSessionInfo resetSharedInstanceForTesting];

  // OS startup time is the same for this and previous session.
  NSTimeInterval current_system_start_time =
      NSDate.timeIntervalSinceReferenceDate -
      NSProcessInfo.processInfo.systemUptime;
  [[NSUserDefaults standardUserDefaults]
      setDouble:current_system_start_time
         forKey:previous_session_info_constants::kOSStartTime];

  EXPECT_FALSE(
      [[PreviousSessionInfo sharedInstance] OSRestartedAfterPreviousSession]);
}

TEST_F(PreviousSessionInfoTest, BeginRecordingCurrentSession) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];

  // Check that the version has been updated.
  EXPECT_NSEQ(base::SysUTF8ToNSString(version_info::GetVersionNumber()),
              [defaults stringForKey:kLastRanVersion]);

  // Check that the memory warning flag has been reset.
  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

TEST_F(PreviousSessionInfoTest, SetMemoryWarningFlagNoOpUntilRecordingBegins) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Call the flag setter.
  [[PreviousSessionInfo sharedInstance] setMemoryWarningFlag];

  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

TEST_F(PreviousSessionInfoTest,
       ResetMemoryWarningFlagNoOpUntilRecordingBegins) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Set the memory warning flag as a previous session would have.
  [defaults setBool:YES forKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Call the memory warning flag resetter.
  [[PreviousSessionInfo sharedInstance] resetMemoryWarningFlag];

  EXPECT_TRUE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

TEST_F(PreviousSessionInfoTest, MemoryWarningFlagMethodsAfterRecordingBegins) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];
  [defaults removeObjectForKey:kLastRanVersion];

  // Launch the recording of the session.
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];

  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);

  // Call the memory warning flag setter.
  [[PreviousSessionInfo sharedInstance] setMemoryWarningFlag];

  EXPECT_TRUE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);

  // Call the memory warning flag resetter.
  [[PreviousSessionInfo sharedInstance] resetMemoryWarningFlag];

  EXPECT_FALSE(
      [defaults boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
}

// Tests restoringSession is in sync with User Defaults.
TEST_F(PreviousSessionInfoTest, NoSessionRestorationInProgress) {
  [PreviousSessionInfo resetSharedInstanceForTesting];

  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:kPreviousSessionInfoRestoringSession];
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];

  EXPECT_FALSE([[PreviousSessionInfo sharedInstance]
      terminatedDuringSessionRestoration]);
}

// Tests restoringSession is in sync with User Defaults.
TEST_F(PreviousSessionInfoTest, SessionRestorationInProgress) {
  [PreviousSessionInfo resetSharedInstanceForTesting];

  [NSUserDefaults.standardUserDefaults
      setBool:YES
       forKey:kPreviousSessionInfoRestoringSession];
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];

  EXPECT_TRUE([[PreviousSessionInfo sharedInstance]
      terminatedDuringSessionRestoration]);
}

// Tests that resetSessionRestorationFlag resets User Defaults.
TEST_F(PreviousSessionInfoTest, ResetSessionRestorationFlag) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [NSUserDefaults.standardUserDefaults
      setBool:YES
       forKey:kPreviousSessionInfoRestoringSession];
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];

  ASSERT_TRUE([NSUserDefaults.standardUserDefaults
      boolForKey:kPreviousSessionInfoRestoringSession]);
  EXPECT_TRUE([[PreviousSessionInfo sharedInstance]
      terminatedDuringSessionRestoration]);

  [[PreviousSessionInfo sharedInstance] resetSessionRestorationFlag];

  EXPECT_FALSE([NSUserDefaults.standardUserDefaults
      boolForKey:kPreviousSessionInfoRestoringSession]);
  EXPECT_FALSE([[PreviousSessionInfo sharedInstance]
      terminatedDuringSessionRestoration]);
}

// Tests that AddSceneSessionID adds to User Defaults.
TEST_F(PreviousSessionInfoTest, AddSceneSessionID) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [[PreviousSessionInfo sharedInstance] addSceneSessionID:kTestSession1ID];
  [[PreviousSessionInfo sharedInstance] addSceneSessionID:kTestSession2ID];
  NSArray<NSString*>* sessionIDs = [NSUserDefaults.standardUserDefaults
      stringArrayForKey:kPreviousSessionInfoConnectedSceneSessionIDs];
  EXPECT_TRUE([sessionIDs containsObject:kTestSession1ID]);
  EXPECT_TRUE([sessionIDs containsObject:kTestSession2ID]);
  EXPECT_EQ(2U, [sessionIDs count]);
}

// Tests that RemoveSceneSessionID removes id from User Defaults.
TEST_F(PreviousSessionInfoTest, RemoveSceneSessionID) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [[PreviousSessionInfo sharedInstance] addSceneSessionID:kTestSession1ID];
  [[PreviousSessionInfo sharedInstance] addSceneSessionID:kTestSession2ID];
  [[PreviousSessionInfo sharedInstance] addSceneSessionID:kTestSession3ID];
  NSArray<NSString*>* sessionIDs = [NSUserDefaults.standardUserDefaults
      stringArrayForKey:kPreviousSessionInfoConnectedSceneSessionIDs];
  ASSERT_EQ(3U, [sessionIDs count]);
  [[PreviousSessionInfo sharedInstance] removeSceneSessionID:kTestSession3ID];
  [[PreviousSessionInfo sharedInstance] removeSceneSessionID:kTestSession1ID];
  sessionIDs = [NSUserDefaults.standardUserDefaults
      stringArrayForKey:kPreviousSessionInfoConnectedSceneSessionIDs];
  EXPECT_FALSE([sessionIDs containsObject:kTestSession3ID]);
  EXPECT_FALSE([sessionIDs containsObject:kTestSession1ID]);
  EXPECT_EQ(1U, [sessionIDs count]);
}

// Tests that resetConnectedSceneSessionIDs remove all session ids from User
// Defaults.
TEST_F(PreviousSessionInfoTest, resetConnectedSceneSessionIDs) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [[PreviousSessionInfo sharedInstance] addSceneSessionID:kTestSession1ID];
  [[PreviousSessionInfo sharedInstance] addSceneSessionID:kTestSession2ID];
  [[PreviousSessionInfo sharedInstance] addSceneSessionID:kTestSession3ID];
  NSArray<NSString*>* sessionIDs = [NSUserDefaults.standardUserDefaults
      stringArrayForKey:kPreviousSessionInfoConnectedSceneSessionIDs];
  ASSERT_EQ(3U, [sessionIDs count]);
  [[PreviousSessionInfo sharedInstance] resetConnectedSceneSessionIDs];
  sessionIDs = [NSUserDefaults.standardUserDefaults
      stringArrayForKey:kPreviousSessionInfoConnectedSceneSessionIDs];
  EXPECT_EQ(0U, [sessionIDs count]);
}

// Tests that scoped object returned from startSessionRestoration correctly
// resets User Defaults.
TEST_F(PreviousSessionInfoTest, ParallelSessionRestorations) {
  [PreviousSessionInfo resetSharedInstanceForTesting];

  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:kPreviousSessionInfoRestoringSession];
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  ASSERT_FALSE([[PreviousSessionInfo sharedInstance]
      terminatedDuringSessionRestoration]);

  {
    base::ScopedClosureRunner scoped_restoration =
        [[PreviousSessionInfo sharedInstance] startSessionRestoration];
    EXPECT_TRUE([NSUserDefaults.standardUserDefaults
        boolForKey:kPreviousSessionInfoRestoringSession]);
    // This should reset to NO after beginRecordingCurrentSession or
    // resetSessionRestorationFlag
    EXPECT_FALSE([[PreviousSessionInfo sharedInstance]
        terminatedDuringSessionRestoration]);
    {
      base::ScopedClosureRunner scoped_restoration2 =
          [[PreviousSessionInfo sharedInstance] startSessionRestoration];
      EXPECT_TRUE([NSUserDefaults.standardUserDefaults
          boolForKey:kPreviousSessionInfoRestoringSession]);
      // This should reset to NO after beginRecordingCurrentSession or
      // resetSessionRestorationFlag
      EXPECT_FALSE([[PreviousSessionInfo sharedInstance]
          terminatedDuringSessionRestoration]);
    }
    EXPECT_TRUE([NSUserDefaults.standardUserDefaults
        boolForKey:kPreviousSessionInfoRestoringSession]);
    // This should reset to NO after beginRecordingCurrentSession or
    // resetSessionRestorationFlag
    EXPECT_FALSE([[PreviousSessionInfo sharedInstance]
        terminatedDuringSessionRestoration]);
  }
  EXPECT_FALSE([NSUserDefaults.standardUserDefaults
      boolForKey:kPreviousSessionInfoRestoringSession]);
  EXPECT_FALSE([[PreviousSessionInfo sharedInstance]
      terminatedDuringSessionRestoration]);
}

// Tests that resetSessionRestorationFlag resets the flag during session
// restoration and that flag is kept reset after restoration is finished.
TEST_F(PreviousSessionInfoTest,
       ResetSessionRestorationFlagDuringParallelSessionRestorations) {
  [PreviousSessionInfo resetSharedInstanceForTesting];

  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:kPreviousSessionInfoRestoringSession];
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  ASSERT_FALSE([[PreviousSessionInfo sharedInstance]
      terminatedDuringSessionRestoration]);

  {
    base::ScopedClosureRunner scoped_restoration =
        [[PreviousSessionInfo sharedInstance] startSessionRestoration];
    EXPECT_TRUE([NSUserDefaults.standardUserDefaults
        boolForKey:kPreviousSessionInfoRestoringSession]);
    // This should reset to NO after beginRecordingCurrentSession or
    // resetSessionRestorationFlag
    EXPECT_FALSE([[PreviousSessionInfo sharedInstance]
        terminatedDuringSessionRestoration]);
    {
      base::ScopedClosureRunner scoped_restoration2 =
          [[PreviousSessionInfo sharedInstance] startSessionRestoration];
      EXPECT_TRUE([NSUserDefaults.standardUserDefaults
          boolForKey:kPreviousSessionInfoRestoringSession]);
      // This should reset to NO after beginRecordingCurrentSession or
      // resetSessionRestorationFlag
      EXPECT_FALSE([[PreviousSessionInfo sharedInstance]
          terminatedDuringSessionRestoration]);

      [[PreviousSessionInfo sharedInstance] resetSessionRestorationFlag];
      EXPECT_FALSE([[PreviousSessionInfo sharedInstance]
          terminatedDuringSessionRestoration]);
      EXPECT_FALSE([NSUserDefaults.standardUserDefaults
          boolForKey:kPreviousSessionInfoRestoringSession]);
    }
    // scoped_restoration2 should not set |restoringSession| to previous state
    // (YES), but rather leave the reset state.
    EXPECT_FALSE([NSUserDefaults.standardUserDefaults
        boolForKey:kPreviousSessionInfoRestoringSession]);
    EXPECT_FALSE([[PreviousSessionInfo sharedInstance]
        terminatedDuringSessionRestoration]);
  }
  EXPECT_FALSE([NSUserDefaults.standardUserDefaults
      boolForKey:kPreviousSessionInfoRestoringSession]);
  EXPECT_FALSE([[PreviousSessionInfo sharedInstance]
      terminatedDuringSessionRestoration]);
}

// Tests adding and removing report parameters.
TEST_F(PreviousSessionInfoTest, ReportParameters) {
  // Default state.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  for (NSString* key in [defaults dictionaryRepresentation].allKeys) {
    if ([key hasPrefix:kPreviousSessionInfoParamsPrefix]) {
      [defaults removeObjectForKey:key];
    }
  }

  [PreviousSessionInfo resetSharedInstanceForTesting];
  EXPECT_EQ([PreviousSessionInfo sharedInstance].reportParameters.count, 0ul);

  // Removing non-existing key does not crash.
  NSString* const kKey0 = @"url0";
  [[PreviousSessionInfo sharedInstance] removeReportParameterForKey:kKey0];
  [PreviousSessionInfo resetSharedInstanceForTesting];
  EXPECT_EQ([PreviousSessionInfo sharedInstance].reportParameters.count, 0ul);

  // Add first URL.
  [[PreviousSessionInfo sharedInstance]
      setReportParameterURL:GURL("https://example.test/path")
                     forKey:kKey0];
  NSDictionary<NSString*, NSString*>* URLs = GetParamsDictionary();
  EXPECT_NSEQ(@{kKey0 : @"https://example.test/"}, URLs);  // stores only origin
  [PreviousSessionInfo resetSharedInstanceForTesting];
  EXPECT_NSEQ(URLs, [[PreviousSessionInfo sharedInstance] reportParameters]);

  // Update first URL.
  [[PreviousSessionInfo sharedInstance]
      setReportParameterURL:GURL("https://example2.test/path")
                     forKey:kKey0];
  URLs = GetParamsDictionary();
  EXPECT_NSEQ(@{kKey0 : @"https://example2.test/"}, URLs);

  // Add second URL.
  NSString* const kKey1 = @"url1";
  [[PreviousSessionInfo sharedInstance]
      setReportParameterURL:GURL("https://example3.test/path")
                     forKey:kKey1];
  URLs = GetParamsDictionary();
  NSDictionary<NSString*, NSString*>* expected = @{
    kKey0 : @"https://example2.test/",
    kKey1 : @"https://example3.test/",
  };
  EXPECT_NSEQ(expected, URLs);

  // Removing non-existing key does not crash.
  [[PreviousSessionInfo sharedInstance] removeReportParameterForKey:@"url2"];

  // Remove first URL.
  [[PreviousSessionInfo sharedInstance] removeReportParameterForKey:kKey0];
  URLs = GetParamsDictionary();
  EXPECT_NSEQ(@{kKey1 : @"https://example3.test/"}, URLs);

  // Remove second URL.
  [[PreviousSessionInfo sharedInstance] removeReportParameterForKey:kKey1];
  URLs = GetParamsDictionary();
  EXPECT_EQ(URLs.count, 0ul);
  [PreviousSessionInfo resetSharedInstanceForTesting];
  EXPECT_EQ([PreviousSessionInfo sharedInstance].reportParameters.count, 0ul);
  [PreviousSessionInfo resetSharedInstanceForTesting];

  // Write a param with spaces, and other non-standard characters.
  NSString* const kAtypicalKey1 = @"* \xe2\x99\xa0";
  NSString* const kAtypicalKey2 = @"http:// not a url.";
  NSString* const kAtypicalKey3 =
      @"\xef\xbf\xbd,\xef\xbf\xbd,\xf0\x90\x8c\x80z,\xef\xbf\xbds";
  [[PreviousSessionInfo sharedInstance] setReportParameterValue:kAtypicalKey1
                                                         forKey:kAtypicalKey1];
  [[PreviousSessionInfo sharedInstance] setReportParameterValue:kAtypicalKey2
                                                         forKey:kAtypicalKey2];
  [[PreviousSessionInfo sharedInstance] setReportParameterValue:kAtypicalKey3
                                                         forKey:kAtypicalKey3];
  expected = @{
    kAtypicalKey1 : kAtypicalKey1,
    kAtypicalKey2 : kAtypicalKey2,
    kAtypicalKey3 : kAtypicalKey3,
  };
  EXPECT_NSEQ(expected, GetParamsDictionary());

  [[PreviousSessionInfo sharedInstance]
      removeReportParameterForKey:kAtypicalKey1];
  [[PreviousSessionInfo sharedInstance]
      removeReportParameterForKey:kAtypicalKey2];
  [[PreviousSessionInfo sharedInstance]
      removeReportParameterForKey:kAtypicalKey3];
  [PreviousSessionInfo resetSharedInstanceForTesting];
  EXPECT_EQ([PreviousSessionInfo sharedInstance].reportParameters.count, 0ul);
  [PreviousSessionInfo resetSharedInstanceForTesting];
}

// Tests that memory footprint gets written to NSUserDefaults after
// startRecordingMemoryFootprintWithInterval: call.
TEST_F(PreviousSessionInfoTest, MemoryFootprintRecording) {
  web::WebTaskEnvironment task_environment;
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:kPreviousSessionInfoMemoryFootprint];

  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  [[PreviousSessionInfo sharedInstance]
      startRecordingMemoryFootprintWithInterval:base::Milliseconds(1)];

  // Memory footprint should be updated after timeout.
  EXPECT_FALSE([NSUserDefaults.standardUserDefaults
      objectForKey:kPreviousSessionInfoMemoryFootprint]);
  EXPECT_TRUE(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1), ^bool {
        base::RunLoop().RunUntilIdle();
        return [[NSUserDefaults.standardUserDefaults
                   objectForKey:kPreviousSessionInfoMemoryFootprint]
                   integerValue] > 0;
      }));
}

// Tests tabCount property.
TEST_F(PreviousSessionInfoTest, TabCount) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [NSUserDefaults.standardUserDefaults setInteger:kTabCount
                                           forKey:kPreviousSessionInfoTabCount];

  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  EXPECT_EQ(kTabCount, [PreviousSessionInfo sharedInstance].tabCount);
}

// Tests tab count gets written to NSUserDefaults.
TEST_F(PreviousSessionInfoTest, TabCountRecording) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:kPreviousSessionInfoTabCount];

  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  [[PreviousSessionInfo sharedInstance] updateCurrentSessionTabCount:kTabCount];

  EXPECT_NSEQ(@(kTabCount), [NSUserDefaults.standardUserDefaults
                                objectForKey:kPreviousSessionInfoTabCount]);
}

// Tests inactiveTabCount property.
TEST_F(PreviousSessionInfoTest, InactiveTabCount) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [NSUserDefaults.standardUserDefaults
      setInteger:kInactiveTabCount
          forKey:kPreviousSessionInfoInactiveTabCount];

  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  EXPECT_EQ(kInactiveTabCount,
            [PreviousSessionInfo sharedInstance].inactiveTabCount);
}

// Tests inactive tab count gets written to NSUserDefaults.
TEST_F(PreviousSessionInfoTest, InactiveTabCountRecording) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:kPreviousSessionInfoInactiveTabCount];

  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  [[PreviousSessionInfo sharedInstance]
      updateCurrentSessionInactiveTabCount:kInactiveTabCount];

  EXPECT_NSEQ(@(kInactiveTabCount),
              [NSUserDefaults.standardUserDefaults
                  objectForKey:kPreviousSessionInfoInactiveTabCount]);
}

// Tests OTRTabCount property.
TEST_F(PreviousSessionInfoTest, OtrTabCount) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [NSUserDefaults.standardUserDefaults
      setInteger:kTabCount
          forKey:kPreviousSessionInfoOTRTabCount];

  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  EXPECT_EQ(kTabCount, [PreviousSessionInfo sharedInstance].OTRTabCount);
}

// Tests OTR tab count gets written to NSUserDefaults.
TEST_F(PreviousSessionInfoTest, OtrTabCountRecording) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:kPreviousSessionInfoOTRTabCount];

  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  [[PreviousSessionInfo sharedInstance]
      updateCurrentSessionOTRTabCount:kTabCount];

  EXPECT_NSEQ(@(kTabCount), [NSUserDefaults.standardUserDefaults
                                objectForKey:kPreviousSessionInfoOTRTabCount]);
}

// Tests memoryFootprint property.
TEST_F(PreviousSessionInfoTest, MemoryFootprint) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  NSInteger kMemoryFootprint = 1869;
  [NSUserDefaults.standardUserDefaults
      setInteger:kMemoryFootprint
          forKey:kPreviousSessionInfoMemoryFootprint];

  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  EXPECT_EQ(kMemoryFootprint,
            [PreviousSessionInfo sharedInstance].memoryFootprint);
}

// Tests data collection pausing.
TEST_F(PreviousSessionInfoTest, PausePreviousSessionInfoCollection) {
  // Default state.
  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:previous_session_info_constants::
                             kPreviousSessionInfoApplicationState];

  [PreviousSessionInfo resetSharedInstanceForTesting];
  EXPECT_FALSE([NSUserDefaults.standardUserDefaults
      valueForKey:previous_session_info_constants::
                      kPreviousSessionInfoApplicationState]);

  // Start recording. This should update the state.
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  EXPECT_TRUE([NSUserDefaults.standardUserDefaults
      valueForKey:previous_session_info_constants::
                      kPreviousSessionInfoApplicationState]);

  // Cleanup.
  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:previous_session_info_constants::
                             kPreviousSessionInfoApplicationState];
  EXPECT_FALSE([NSUserDefaults.standardUserDefaults
      valueForKey:previous_session_info_constants::
                      kPreviousSessionInfoApplicationState]);

  // Updating state should work when recording is enabled.
  [[PreviousSessionInfo sharedInstance] updateApplicationState];
  EXPECT_TRUE([NSUserDefaults.standardUserDefaults
      valueForKey:previous_session_info_constants::
                      kPreviousSessionInfoApplicationState]);

  // Cleanup.
  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:previous_session_info_constants::
                             kPreviousSessionInfoApplicationState];
  EXPECT_FALSE([NSUserDefaults.standardUserDefaults
      valueForKey:previous_session_info_constants::
                      kPreviousSessionInfoApplicationState]);

  // Updating state should be noop when recording is paused.
  [[PreviousSessionInfo sharedInstance] pauseRecordingCurrentSession];
  [[PreviousSessionInfo sharedInstance] updateApplicationState];
  EXPECT_FALSE([NSUserDefaults.standardUserDefaults
      valueForKey:previous_session_info_constants::
                      kPreviousSessionInfoApplicationState]);

  // Resume recording should update the state.
  [[PreviousSessionInfo sharedInstance] resumeRecordingCurrentSession];
  EXPECT_TRUE([NSUserDefaults.standardUserDefaults
      valueForKey:previous_session_info_constants::
                      kPreviousSessionInfoApplicationState]);

  // Cleanup
  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:previous_session_info_constants::
                             kPreviousSessionInfoApplicationState];
  EXPECT_FALSE([NSUserDefaults.standardUserDefaults
      valueForKey:previous_session_info_constants::
                      kPreviousSessionInfoApplicationState]);

  // Updating state should work when recording is enabled.
  [[PreviousSessionInfo sharedInstance] updateApplicationState];
  EXPECT_TRUE([NSUserDefaults.standardUserDefaults
      valueForKey:previous_session_info_constants::
                      kPreviousSessionInfoApplicationState]);

  // Cleanup.
  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:previous_session_info_constants::
                             kPreviousSessionInfoApplicationState];
  [PreviousSessionInfo resetSharedInstanceForTesting];
}

}  // namespace
