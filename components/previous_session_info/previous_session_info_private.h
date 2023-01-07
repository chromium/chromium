// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIOUS_SESSION_INFO_PREVIOUS_SESSION_INFO_PRIVATE_H_
#define COMPONENTS_PREVIOUS_SESSION_INFO_PREVIOUS_SESSION_INFO_PRIVATE_H_

#import "components/previous_session_info/previous_session_info.h"

@interface PreviousSessionInfo (TestingOnly)

// Redefined to be read-write.
@property(nonatomic, assign) NSInteger availableDeviceStorage;
@property(nonatomic, assign) BOOL didSeeMemoryWarningShortlyBeforeTerminating;
@property(nonatomic, assign) BOOL isFirstSessionAfterUpgrade;
@property(nonatomic, assign) float deviceBatteryLevel;
@property(nonatomic, assign)
    previous_session_info_constants::DeviceBatteryState deviceBatteryState;
@property(nonatomic, assign) BOOL OSRestartedAfterPreviousSession;
@property(nonatomic, copy) NSString* OSVersion;
@property(nonatomic, strong) NSDate* sessionStartTime;
@property(nonatomic, strong) NSDate* sessionEndTime;
@property(nonatomic, assign) BOOL terminatedDuringSessionRestoration;
@property(nonatomic, strong) NSMutableSet<NSString*>* connectedSceneSessionsIDs;
@property(nonatomic, copy) NSDictionary<NSString*, NSString*>* reportParameters;
@property(nonatomic, assign) NSInteger memoryFootprint;
@property(nonatomic, assign) NSInteger tabCount;
@property(nonatomic, assign) NSInteger OTRTabCount;
@property(nonatomic, assign) BOOL applicationWillTerminateWasReceived;

+ (void)resetSharedInstanceForTesting;

- (void)pauseRecordingCurrentSession;
- (void)resumeRecordingCurrentSession;
- (void)updateApplicationState;

@end

#endif  // COMPONENTS_PREVIOUS_SESSION_INFO_PREVIOUS_SESSION_INFO_PRIVATE_H_
