// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIOUS_SESSION_INFO_PREVIOUS_SESSION_INFO_H_
#define COMPONENTS_PREVIOUS_SESSION_INFO_PREVIOUS_SESSION_INFO_H_

#import <UIKit/UIKit.h>

#include "base/functional/callback_helpers.h"

#include "url/gurl.h"

namespace base {
class TimeDelta;
}

namespace previous_session_info_constants {
// - The (Integer) representing UIApplicationState.
extern NSString* const kPreviousSessionInfoApplicationState;
// Key in the UserDefaults for a boolean value keeping track of memory warnings.
extern NSString* const kDidSeeMemoryWarningShortlyBeforeTerminating;
// Key in the UserDefaults for a double value which stores OS start time.
extern NSString* const kOSStartTime;
// Key in the UserDefaults for a boolean describing whether or not the session
// restoration is in progress.
extern NSString* const kPreviousSessionInfoRestoringSession;
// Key in the UserDefaults for an array which contains the ids for the connected
// scene sessions on the previous run.
extern NSString* const kPreviousSessionInfoConnectedSceneSessionIDs;
// Prefix key in the UserDefaults for a dictionary with session info params.
extern NSString* const kPreviousSessionInfoParamsPrefix;
// Key in the UserDefaults for the memory footprint of the browser process.
extern NSString* const kPreviousSessionInfoMemoryFootprint;
// Key in the UserDefaults for the number of open tabs.
extern NSString* const kPreviousSessionInfoTabCount;
// Key in the UserDefaults for the number of open inactive tabs.
extern NSString* const kPreviousSessionInfoInactiveTabCount;
// Key in the UserDefaults for the number of open "off the record" tabs.
extern NSString* const kPreviousSessionInfoOTRTabCount;

// The values of this enum are persisted (both to NSUserDefaults and logs) and
// represent the state of the last session (which may have been running a
// different version of the application).
// Therefore, entries should not be renumbered and numeric values should never
// be reused.
enum class DeviceThermalState {
  kUnknown = 0,
  kNominal = 1,
  kFair = 2,
  kSerious = 3,
  kCritical = 4,
  kMaxValue = kCritical,
};

// The values of this enum are persisted (both to NSUserDefaults and logs) and
// represent the state of the last session (which may have been running a
// different version of the application).
// Therefore, entries should not be renumbered and numeric values should never
// be reused.
enum class DeviceBatteryState {
  kUnknown = 0,
  kUnplugged = 1,
  kCharging = 2,
  // Battery is plugged into power and the battery is 100% charged.
  kFull = 3,
  kMaxValue = kFull,
};
}  // namespace previous_session_info_constants

// PreviousSessionInfo has two jobs:
// - Holding information about the last session, persisted across restart.
//   These informations are accessible via the properties on the shared
//   instance.
// - Persist information about the current session, for use in a next session.
@interface PreviousSessionInfo : NSObject

// UIApplicationState at the end of the previous session or nil if state is
// unknown.
@property(nonatomic, assign, readonly) UIApplicationState* applicationState;

// The battery level of the device at the end of the previous session.
@property(nonatomic, assign, readonly) float deviceBatteryLevel;

// The battery state of the device at the end of the previous session.
@property(nonatomic, assign, readonly)
    previous_session_info_constants::DeviceBatteryState deviceBatteryState;

// The storage available, in kilobytes, at the end of the previous session or -1
// if no previous session data is available.
@property(nonatomic, assign, readonly) NSInteger availableDeviceStorage;

// The thermal state of the device at the end of the previous session.
@property(nonatomic, assign, readonly)
    previous_session_info_constants::DeviceThermalState deviceThermalState;

// Whether the app received a memory warning seconds before being terminated.
@property(nonatomic, assign, readonly)
    BOOL didSeeMemoryWarningShortlyBeforeTerminating;

// Whether the app was updated between the previous and the current session.
@property(nonatomic, assign, readonly) BOOL isFirstSessionAfterUpgrade;

// Whether the language has been changed between the previous and the current
// session.
@property(nonatomic, assign, readonly) BOOL isFirstSessionAfterLanguageChange;

// Whether or not the OS was restarted between the previous and the current
// session.
@property(nonatomic, assign, readonly) BOOL OSRestartedAfterPreviousSession;

// The OS version during the previous session or nil if no previous session data
// is available.
@property(nonatomic, strong, readonly) NSString* OSVersion;

// The date time at which recording for the previous sesion has started. Note
// that recording usually starts soon after startup, but not exactly at the
// startup.
@property(nonatomic, strong, readonly) NSDate* sessionStartTime;

// The time at which the previous sesion ended. Note that this is only an
// estimate and is updated whenever another value of the receiver is updated.
@property(nonatomic, strong, readonly) NSDate* sessionEndTime;

// YES if the previous session was terminated during session restoration.
// Reset to NO after resetSessionRestorationFlag call.
@property(nonatomic, readonly) BOOL terminatedDuringSessionRestoration;

// The list of the session IDs for all the connected scenes, used for crash
// restoration.
@property(nonatomic, readonly)
    NSMutableSet<NSString*>* connectedSceneSessionsIDs;

// Crash report parameters as key-value pairs.
@property(atomic, readonly)
    NSDictionary<NSString*, NSString*>* reportParameters;

// Memory footprint in bytes of the browser process.
@property(nonatomic, readonly) NSInteger memoryFootprint;

// YES if ApplicationWillTerminate notification was posted for the previous
// session.
@property(nonatomic, readonly) BOOL applicationWillTerminateWasReceived;

// Number of open tabs in the previous session.
@property(nonatomic, readonly) NSInteger tabCount;

// Number of open inactive tabs in the previous session.
@property(nonatomic, readonly) NSInteger inactiveTabCount;

// Number of open "off the record" tabs in the previous session.
@property(nonatomic, readonly) NSInteger OTRTabCount;

// The breadcrumbs from the previous session.
@property(atomic, readonly) NSString* breadcrumbs;

// Number of warm starts in the previous session.
@property(nonatomic, readonly) NSInteger warmStartCount;

// Singleton PreviousSessionInfo. During the lifetime of the app, the returned
// object is the same, and describes the previous session, even after a new
// session has started (by calling beginRecordingCurrentSession).
+ (instancetype)sharedInstance;

// Clears the persisted information about the previous session and starts
// persisting information about the current session, for use in a next session.
- (void)beginRecordingCurrentSession;

// Start recording active field trials.
- (void)beginRecordingFieldTrials;

// Starts memory usage data recording with given |interval|.
- (void)startRecordingMemoryFootprintWithInterval:(base::TimeDelta)interval;

// Stops memory usage data recording. No-op if
// startRecordingMemoryFootprintWithInterval was no called.
- (void)stopRecordingMemoryFootprint;

// Updates the saved last known session time.
- (void)updateSessionEndTime;

// Updates the saved last known battery level of the device.
- (void)updateStoredBatteryLevel;

// Updates the saved last known battery state of the device.
- (void)updateStoredBatteryState;

// Updates the saved last known thermal state of the device.
- (void)updateStoredThermalState;

// When a session has begun, records that a memory warning was received.
- (void)setMemoryWarningFlag;

// When a session has begun, records that any memory warning flagged can be
// ignored.
- (void)resetMemoryWarningFlag;

// Adds |sessionID| to the list of connected sessions.
- (void)addSceneSessionID:(NSString*)sessionID;

// Removes |sessionID| from the list of connected sessions.
- (void)removeSceneSessionID:(NSString*)sessionID;

// Empties the list of connected session.
- (void)resetConnectedSceneSessionIDs;

// Increments the warm start count by one.
- (void)incrementWarmStartCount;

// Resets the warm start count to zero.
- (void)resetWarmStartCount;

// Must be called when Chrome starts session restoration. The returned closure
// runner will clear up the flag when destroyed. Can be used on different
// threads.
- (base::ScopedClosureRunner)startSessionRestoration;

// Must be called after reporting UTE metrics when app is started after UTE.
// Automatically called when ScopedClosureRunner returned from -startRestoration
// gets destructed.
- (void)resetSessionRestorationFlag;

// Records number of regular (non off the record and non inactive) tabs.
- (void)updateCurrentSessionTabCount:(NSInteger)count;
// Records number of inactive tabs.
- (void)updateCurrentSessionInactiveTabCount:(NSInteger)count;
// Records number of off the record tabs.
- (void)updateCurrentSessionOTRTabCount:(NSInteger)count;

// Records breadcrumbs from the previous session.
- (void)setBreadcrumbsLog:(NSString*)breadcrumbs;

// Records information crash report parameters.
- (void)setReportParameterValue:(NSString*)value forKey:(NSString*)key;
- (void)setReportParameterURL:(const GURL&)URL forKey:(NSString*)key;
- (void)removeReportParameterForKey:(NSString*)key;

@end

#endif  // COMPONENTS_PREVIOUS_SESSION_INFO_PREVIOUS_SESSION_INFO_H_
