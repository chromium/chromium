// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/previous_session_info/previous_session_info.h"

#include <mach/mach.h>

#import <UIKit/UIKit.h>

#import "base/ios/ios_util.h"
#import "base/metrics/field_trial.h"
#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/previous_session_info/previous_session_info_private.h"
#import "components/variations/variations_crash_keys.h"
#import "components/version_info/version_info.h"

using previous_session_info_constants::DeviceBatteryState;
using previous_session_info_constants::DeviceThermalState;
using previous_session_info_constants::kPreviousSessionInfoParamsPrefix;

namespace {

// Returns timestamp (in seconds since January 2001) when OS has started.
NSTimeInterval GetOSStartTimeIntervalSinceReferenceDate() {
  return NSDate.timeIntervalSinceReferenceDate -
         NSProcessInfo.processInfo.systemUptime;
}

// Translates a UIDeviceBatteryState value to DeviceBatteryState value.
DeviceBatteryState GetBatteryStateFromUIDeviceBatteryState(
    UIDeviceBatteryState device_battery_state) {
  switch (device_battery_state) {
    case UIDeviceBatteryStateUnknown:
      return DeviceBatteryState::kUnknown;
    case UIDeviceBatteryStateUnplugged:
      return DeviceBatteryState::kUnplugged;
    case UIDeviceBatteryStateCharging:
      return DeviceBatteryState::kCharging;
    case UIDeviceBatteryStateFull:
      return DeviceBatteryState::kFull;
  }

  return DeviceBatteryState::kUnknown;
}

// Translates a NSProcessInfoThermalState value to DeviceThermalState value.
DeviceThermalState GetThermalStateFromNSProcessInfoThermalState(
    NSProcessInfoThermalState process_info_thermal_state) {
  switch (process_info_thermal_state) {
    case NSProcessInfoThermalStateNominal:
      return DeviceThermalState::kNominal;
    case NSProcessInfoThermalStateFair:
      return DeviceThermalState::kFair;
    case NSProcessInfoThermalStateSerious:
      return DeviceThermalState::kSerious;
    case NSProcessInfoThermalStateCritical:
      return DeviceThermalState::kCritical;
  }

  return DeviceThermalState::kUnknown;
}

// NSUserDefaults keys.
// - The (string) application version.
NSString* const kLastRanVersion = @"LastRanVersion";
// - The (string) device language.
NSString* const kLastRanLanguage = @"LastRanLanguage";
// - The (integer) available device storage, in kilobytes.
NSString* const kPreviousSessionInfoAvailableDeviceStorage =
    @"PreviousSessionInfoAvailableDeviceStorage";
// - The (float) battery charge level.
NSString* const kPreviousSessionInfoBatteryLevel =
    @"PreviousSessionInfoBatteryLevel";
// - The (integer) underlying value of the DeviceBatteryState enum representing
//   the device battery state.
NSString* const kPreviousSessionInfoBatteryState =
    @"PreviousSessionInfoBatteryState";
// - The (Date) of the recording start.
NSString* const kPreviousSessionInfoStartTime = @"PreviousSessionInfoStartTime";
// - The (Date) of the estimated end of the session.
NSString* const kPreviousSessionInfoEndTime = @"PreviousSessionInfoEndTime";
// - The (string) OS version.
NSString* const kPreviousSessionInfoOSVersion = @"PreviousSessionInfoOSVersion";
// - The (integer) underlying value of the DeviceThermalState enum representing
//   the device thermal state.
NSString* const kPreviousSessionInfoThermalState =
    @"PreviousSessionInfoThermalState";
// - A (boolean) describing whether the last session received
// ApplicationWillTerminate Notification.
NSString* const kPreviousSessionInfoAppWillTerminate =
    @"PreviousSessionInfoAppWillTerminate";

// Return a key prefixed with the params prefix.
NSString* ReportParamKey(NSString* key) {
  return [NSString
      stringWithFormat:@"%@%@", kPreviousSessionInfoParamsPrefix, key];
}

// Objective-C bridge to observe changes in the FieldTrialList.
class FieldTrialListObserverBridge : public base::FieldTrialList::Observer {
 public:
  explicit FieldTrialListObserverBridge() {}

 private:
  FieldTrialListObserverBridge(const PreviousSessionInfo&) = delete;
  FieldTrialListObserverBridge& operator=(const FieldTrialListObserverBridge&) =
      delete;

  // base::FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const base::FieldTrial& trial,
                                  const std::string& group_name) override {
    dispatch_async(dispatch_get_main_queue(), ^{
      variations::ExperimentListInfo info = variations::GetExperimentListInfo();

      // Normally this call would go through -setReportParameterValue, which
      // calls -setObject and -synchronize on NSUserDefaults. However, since
      // this call is really noisy, just call setObject and skip synchronize.
      [NSUserDefaults.standardUserDefaults
          setObject:base::SysUTF8ToNSString(
                        base::NumberToString(info.num_experiments))
             forKey:ReportParamKey(@"num-experiments")];
      [NSUserDefaults.standardUserDefaults
          setObject:base::SysUTF8ToNSString(info.experiment_list)
             forKey:ReportParamKey(@"variations")];
    });
  }
};

}  // namespace

namespace previous_session_info_constants {
NSString* const kPreviousSessionInfoApplicationState =
    @"PreviousSessionInfoApplicationState";
NSString* const kDidSeeMemoryWarningShortlyBeforeTerminating =
    @"DidSeeMemoryWarning";
NSString* const kOSStartTime = @"OSStartTime";
NSString* const kPreviousSessionInfoRestoringSession =
    @"PreviousSessionInfoRestoringSession";
NSString* const kPreviousSessionInfoConnectedSceneSessionIDs =
    @"PreviousSessionInfoConnectedSceneSessionIDs";
NSString* const kPreviousSessionInfoParamsPrefix =
    @"PreviousSessionInfoParams.";
NSString* const kPreviousSessionInfoMemoryFootprint =
    @"PreviousSessionInfoMemoryFootprint";
NSString* const kPreviousSessionInfoTabCount = @"PreviousSessionInfoTabCount";
NSString* const kPreviousSessionInfoInactiveTabCount =
    @"PreviousSessionInfoInactiveTabCount";
NSString* const kPreviousSessionInfoOTRTabCount =
    @"PreviousSessionInfoOTRTabCount";
NSString* const kPreviousSessionInfoWarmStartCount =
    @"PreviousSessionInfoWarmStartCount";
}  // namespace previous_session_info_constants

@interface PreviousSessionInfo () {
  // Observe updates to field trial list.
  std::unique_ptr<FieldTrialListObserverBridge> _fieldTrialListObserver;
}

// Whether beginRecordingCurrentSession was called.
@property(nonatomic, assign) BOOL didBeginRecordingCurrentSession;

// Whether recording data is in progress.
@property(nonatomic, assign) BOOL recordingCurrentSession;

// Used for setting and resetting kPreviousSessionInfoRestoringSession flag.
// Can be greater than one if multiple sessions are being restored in parallel.
@property(atomic, assign) int numberOfSessionsBeingRestored;

// Redefined to be read-write.
@property(nonatomic, assign) NSInteger availableDeviceStorage;
@property(nonatomic, assign) float deviceBatteryLevel;
@property(nonatomic, assign) DeviceBatteryState deviceBatteryState;
@property(nonatomic, assign) DeviceThermalState deviceThermalState;
@property(nonatomic, assign) BOOL didSeeMemoryWarningShortlyBeforeTerminating;
@property(nonatomic, assign) BOOL isFirstSessionAfterUpgrade;
@property(nonatomic, assign) BOOL isFirstSessionAfterLanguageChange;
@property(nonatomic, assign) BOOL OSRestartedAfterPreviousSession;
@property(nonatomic, strong) NSString* OSVersion;
@property(nonatomic, strong) NSDate* sessionStartTime;
@property(nonatomic, strong) NSDate* sessionEndTime;
@property(nonatomic, assign) BOOL terminatedDuringSessionRestoration;
@property(nonatomic, strong) NSMutableSet<NSString*>* connectedSceneSessionsIDs;
@property(atomic, copy) NSDictionary<NSString*, NSString*>* reportParameters;
@property(nonatomic, assign) NSInteger memoryFootprint;
@property(nonatomic, assign) BOOL applicationWillTerminateWasReceived;
@property(nonatomic, assign) NSInteger tabCount;
@property(nonatomic, assign) NSInteger inactiveTabCount;
@property(nonatomic, assign) NSInteger OTRTabCount;
@property(atomic, strong) NSString* breadcrumbs;
@property(nonatomic, assign) NSInteger warmStartCount;

@end

@implementation PreviousSessionInfo {
  std::unique_ptr<UIApplicationState> _applicationState;
  base::RepeatingTimer _memoryFootprintUpdateTimer;
}

// Singleton PreviousSessionInfo.
static PreviousSessionInfo* gSharedInstance = nil;

+ (instancetype)sharedInstance {
  if (!gSharedInstance) {
    gSharedInstance = [[PreviousSessionInfo alloc] init];

    // Load the persisted information.
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

    gSharedInstance->_applicationState.reset();
    if ([defaults objectForKey:previous_session_info_constants::
                                   kPreviousSessionInfoApplicationState]) {
      gSharedInstance->_applicationState = std::make_unique<UIApplicationState>(
          static_cast<UIApplicationState>([defaults
              integerForKey:previous_session_info_constants::
                                kPreviousSessionInfoApplicationState]));
    }

    gSharedInstance.availableDeviceStorage = -1;
    if ([defaults objectForKey:kPreviousSessionInfoAvailableDeviceStorage]) {
      gSharedInstance.availableDeviceStorage =
          [defaults integerForKey:kPreviousSessionInfoAvailableDeviceStorage];
    }
    gSharedInstance.didSeeMemoryWarningShortlyBeforeTerminating =
        [defaults boolForKey:previous_session_info_constants::
                                 kDidSeeMemoryWarningShortlyBeforeTerminating];
    gSharedInstance.deviceBatteryState = static_cast<DeviceBatteryState>(
        [defaults integerForKey:kPreviousSessionInfoBatteryState]);
    gSharedInstance.deviceBatteryLevel =
        [defaults floatForKey:kPreviousSessionInfoBatteryLevel];
    gSharedInstance.deviceThermalState = static_cast<DeviceThermalState>(
        [defaults integerForKey:kPreviousSessionInfoThermalState]);
    gSharedInstance.sessionStartTime =
        [defaults objectForKey:kPreviousSessionInfoStartTime];
    gSharedInstance.sessionEndTime =
        [defaults objectForKey:kPreviousSessionInfoEndTime];

    NSString* versionOfOSAtLastRun =
        [defaults stringForKey:kPreviousSessionInfoOSVersion];
    gSharedInstance.OSVersion = versionOfOSAtLastRun;

    NSString* lastRanVersion = [defaults stringForKey:kLastRanVersion];
    NSString* currentVersion =
        base::SysUTF8ToNSString(version_info::GetVersionNumber());
    gSharedInstance.isFirstSessionAfterUpgrade =
        ![lastRanVersion isEqualToString:currentVersion];

    gSharedInstance.connectedSceneSessionsIDs = [NSMutableSet
        setWithArray:[defaults
                         stringArrayForKey:
                             previous_session_info_constants::
                                 kPreviousSessionInfoConnectedSceneSessionIDs]];

    NSTimeInterval lastSystemStartTime =
        [defaults doubleForKey:previous_session_info_constants::kOSStartTime];

    gSharedInstance.OSRestartedAfterPreviousSession =
        // Allow 5 seconds variation to account for rounding error.
        (abs(lastSystemStartTime - GetOSStartTimeIntervalSinceReferenceDate()) >
         5) &&
        // Ensure that previous session actually exists.
        lastSystemStartTime;

    NSString* lastRanLanguage = [defaults stringForKey:kLastRanLanguage];
    NSString* currentLanguage = [[NSLocale preferredLanguages] objectAtIndex:0];
    gSharedInstance.isFirstSessionAfterLanguageChange =
        ![lastRanLanguage isEqualToString:currentLanguage];

    gSharedInstance.terminatedDuringSessionRestoration =
        [defaults boolForKey:previous_session_info_constants::
                                 kPreviousSessionInfoRestoringSession];

    NSMutableDictionary* reportParameters = [[NSMutableDictionary alloc] init];
    NSUInteger prefix_length = kPreviousSessionInfoParamsPrefix.length;
    for (NSString* key in [defaults dictionaryRepresentation].allKeys) {
      if ([key hasPrefix:kPreviousSessionInfoParamsPrefix]) {
        NSString* crash_key = [key substringFromIndex:prefix_length];
        reportParameters[crash_key] = [defaults stringForKey:key];
        [defaults removeObjectForKey:key];
      }
    }
    gSharedInstance.reportParameters = reportParameters;

    gSharedInstance.memoryFootprint =
        [defaults integerForKey:previous_session_info_constants::
                                    kPreviousSessionInfoMemoryFootprint];

    gSharedInstance.applicationWillTerminateWasReceived =
        [defaults boolForKey:kPreviousSessionInfoAppWillTerminate];
    gSharedInstance.tabCount =
        [defaults integerForKey:previous_session_info_constants::
                                    kPreviousSessionInfoTabCount];
    gSharedInstance.inactiveTabCount =
        [defaults integerForKey:previous_session_info_constants::
                                    kPreviousSessionInfoInactiveTabCount];
    gSharedInstance.OTRTabCount =
        [defaults integerForKey:previous_session_info_constants::
                                    kPreviousSessionInfoOTRTabCount];
    gSharedInstance.warmStartCount =
        [defaults integerForKey:previous_session_info_constants::
                                    kPreviousSessionInfoWarmStartCount];
  }
  return gSharedInstance;
}

+ (void)resetSharedInstanceForTesting {
  gSharedInstance = nil;
}

- (void)beginRecordingCurrentSession {
  if (self.didBeginRecordingCurrentSession)
    return;
  self.didBeginRecordingCurrentSession = YES;
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Set the current Chrome version.
  NSString* currentVersion =
      base::SysUTF8ToNSString(version_info::GetVersionNumber());
  [defaults setObject:currentVersion forKey:kLastRanVersion];

  // Set the current OS start time.
  [defaults setDouble:GetOSStartTimeIntervalSinceReferenceDate()
               forKey:previous_session_info_constants::kOSStartTime];

  // Set the current OS version.
  NSString* currentOSVersion =
      base::SysUTF8ToNSString(base::SysInfo::OperatingSystemVersion());
  [defaults setObject:currentOSVersion forKey:kPreviousSessionInfoOSVersion];

  // Set the current language.
  NSString* currentLanguage = [[NSLocale preferredLanguages] objectAtIndex:0];
  [defaults setObject:currentLanguage forKey:kLastRanLanguage];

  // Clear the memory warning flag.
  [defaults
      removeObjectForKey:previous_session_info_constants::
                             kDidSeeMemoryWarningShortlyBeforeTerminating];

  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kPreviousSessionInfoAppWillTerminate];
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kPreviousSessionInfoAvailableDeviceStorage];

  [defaults setObject:[NSDate date] forKey:kPreviousSessionInfoStartTime];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateApplicationState)
             name:UIApplicationDidEnterBackgroundNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateApplicationState)
             name:UIApplicationWillEnterForegroundNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(protectedDataWillBecomeUnavailable)
             name:UIApplicationProtectedDataWillBecomeUnavailable
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(protectedDataDidBecomeAvailable)
             name:UIApplicationProtectedDataWillBecomeUnavailable
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateApplicationState)
             name:UIApplicationDidBecomeActiveNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateApplicationState)
             name:UIApplicationWillResignActiveNotification
           object:nil];

  [UIDevice currentDevice].batteryMonitoringEnabled = YES;

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateStoredBatteryLevel)
             name:UIDeviceBatteryLevelDidChangeNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateStoredBatteryState)
             name:UIDeviceBatteryStateDidChangeNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(updateStoredThermalState)
             name:NSProcessInfoThermalStateDidChangeNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(applicationWillTerminate)
             name:UIApplicationWillTerminateNotification
           object:nil];

  [self resumeRecordingCurrentSession];
}

- (void)beginRecordingFieldTrials {
  _fieldTrialListObserver = std::make_unique<FieldTrialListObserverBridge>();
  bool success =
      base::FieldTrialList::AddObserver(_fieldTrialListObserver.get());
  DCHECK(success);
}

- (void)startRecordingMemoryFootprintWithInterval:(base::TimeDelta)interval {
  _memoryFootprintUpdateTimer.Start(FROM_HERE, interval, base::BindRepeating(^{
                                      [self updateMemoryFootprint];
                                    }));
}

- (void)stopRecordingMemoryFootprint {
  _memoryFootprintUpdateTimer.Stop();
}
- (void)resumeRecordingCurrentSession {
  if (self.recordingCurrentSession)
    return;
  self.recordingCurrentSession = YES;
  [self updateApplicationState];
  [self updateStoredBatteryLevel];
  [self updateStoredBatteryState];
  [self updateStoredThermalState];
  // Save critical state information for crash detection.
  [[NSUserDefaults standardUserDefaults] synchronize];
}

- (void)pauseRecordingCurrentSession {
  self.recordingCurrentSession = NO;
}

- (void)protectedDataWillBecomeUnavailable {
  [self pauseRecordingCurrentSession];
}

- (void)protectedDataDidBecomeAvailable {
  [self resumeRecordingCurrentSession];
}

- (UIApplicationState*)applicationState {
  return _applicationState.get();
}

- (void)updateSessionEndTime {
  if (!self.recordingCurrentSession)
    return;
  [[NSUserDefaults standardUserDefaults] setObject:[NSDate date]
                                            forKey:kPreviousSessionInfoEndTime];
}

- (void)updateStoredBatteryLevel {
  if (!self.recordingCurrentSession)
    return;
  [[NSUserDefaults standardUserDefaults]
      setFloat:[UIDevice currentDevice].batteryLevel
        forKey:kPreviousSessionInfoBatteryLevel];
  [self updateSessionEndTime];
}

- (void)updateApplicationState {
  if (!self.recordingCurrentSession)
    return;
  [[NSUserDefaults standardUserDefaults]
      setInteger:UIApplication.sharedApplication.applicationState
          forKey:previous_session_info_constants::
                     kPreviousSessionInfoApplicationState];

  [self updateSessionEndTime];
}

- (void)updateStoredBatteryState {
  if (!self.recordingCurrentSession)
    return;
  UIDevice* device = [UIDevice currentDevice];
  // Translate value to an app defined enum as the system could change the
  // underlying values of UIDeviceBatteryState between OS versions.
  DeviceBatteryState batteryState =
      GetBatteryStateFromUIDeviceBatteryState(device.batteryState);
  NSInteger batteryStateValue =
      static_cast<std::underlying_type<DeviceBatteryState>::type>(batteryState);

  [[NSUserDefaults standardUserDefaults]
      setInteger:batteryStateValue
          forKey:kPreviousSessionInfoBatteryState];

  [self updateSessionEndTime];
}

- (void)updateStoredThermalState {
  if (!self.recordingCurrentSession)
    return;
  NSProcessInfo* processInfo = [NSProcessInfo processInfo];
  // Translate value to an app defined enum as the system could change the
  // underlying values of NSProcessInfoThermalState between OS versions.
  DeviceThermalState thermalState =
      GetThermalStateFromNSProcessInfoThermalState([processInfo thermalState]);
  NSInteger thermalStateValue =
      static_cast<std::underlying_type<DeviceThermalState>::type>(thermalState);

  [[NSUserDefaults standardUserDefaults]
      setInteger:thermalStateValue
          forKey:kPreviousSessionInfoThermalState];

  [self updateSessionEndTime];
}

- (void)applicationWillTerminate {
  [NSUserDefaults.standardUserDefaults
      setBool:YES
       forKey:kPreviousSessionInfoAppWillTerminate];
  [NSUserDefaults.standardUserDefaults synchronize];
}

- (void)updateMemoryFootprint {
  if (!self.recordingCurrentSession)
    return;

  task_vm_info taskInfoData;
  mach_msg_type_number_t count = sizeof(task_vm_info) / sizeof(natural_t);
  kern_return_t result =
      task_info(mach_task_self(), TASK_VM_INFO,
                reinterpret_cast<task_info_t>(&taskInfoData), &count);
  if (result == KERN_SUCCESS) {
    [NSUserDefaults.standardUserDefaults
        setInteger:taskInfoData.phys_footprint
            forKey:previous_session_info_constants::
                       kPreviousSessionInfoMemoryFootprint];
    [self updateSessionEndTime];
  }
}

- (void)setMemoryWarningFlag {
  if (!self.didBeginRecordingCurrentSession)
    return;

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:YES
             forKey:previous_session_info_constants::
                        kDidSeeMemoryWarningShortlyBeforeTerminating];
  // Save critical state information for crash detection.
  [defaults synchronize];
}

- (void)resetMemoryWarningFlag {
  if (!self.didBeginRecordingCurrentSession)
    return;

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults
      removeObjectForKey:previous_session_info_constants::
                             kDidSeeMemoryWarningShortlyBeforeTerminating];
  // Save critical state information for crash detection.
  [defaults synchronize];
}

- (void)synchronizeSceneSessionIDs {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:[self.connectedSceneSessionsIDs allObjects]
               forKey:previous_session_info_constants::
                          kPreviousSessionInfoConnectedSceneSessionIDs];
  [defaults synchronize];
}

- (void)addSceneSessionID:(NSString*)sessionID {
  [self.connectedSceneSessionsIDs addObject:sessionID];
  [self synchronizeSceneSessionIDs];
}

- (void)removeSceneSessionID:(NSString*)sessionID {
  [self.connectedSceneSessionsIDs removeObject:sessionID];
  [self synchronizeSceneSessionIDs];
}

- (void)resetConnectedSceneSessionIDs {
  self.connectedSceneSessionsIDs = [[NSMutableSet alloc] init];
  [self synchronizeSceneSessionIDs];
}

- (void)incrementWarmStartCount {
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
  NSInteger warmStartCount =
      [defaults integerForKey:previous_session_info_constants::
                                  kPreviousSessionInfoWarmStartCount];
  [defaults setInteger:warmStartCount + 1
                forKey:previous_session_info_constants::
                           kPreviousSessionInfoWarmStartCount];
  [defaults synchronize];
}

- (void)resetWarmStartCount {
  [NSUserDefaults.standardUserDefaults
      setInteger:0
          forKey:previous_session_info_constants::
                     kPreviousSessionInfoWarmStartCount];
  [NSUserDefaults.standardUserDefaults synchronize];
}

- (base::ScopedClosureRunner)startSessionRestoration {
  if (self.numberOfSessionsBeingRestored == 0) {
    [NSUserDefaults.standardUserDefaults
        setBool:YES
         forKey:previous_session_info_constants::
                    kPreviousSessionInfoRestoringSession];
    // Save critical state information for crash detection.
    [NSUserDefaults.standardUserDefaults synchronize];
  }
  ++self.numberOfSessionsBeingRestored;

  return base::ScopedClosureRunner(base::BindOnce(^{
    --self.numberOfSessionsBeingRestored;
    if (self.numberOfSessionsBeingRestored == 0) {
      [self resetSessionRestorationFlag];
    }
  }));
}

- (void)resetSessionRestorationFlag {
  gSharedInstance.terminatedDuringSessionRestoration = NO;
  [NSUserDefaults.standardUserDefaults
      removeObjectForKey:previous_session_info_constants::
                             kPreviousSessionInfoRestoringSession];
  // Save critical state information for crash detection.
  [NSUserDefaults.standardUserDefaults synchronize];
}

- (void)updateCurrentSessionTabCount:(NSInteger)count {
  [NSUserDefaults.standardUserDefaults
      setInteger:count
          forKey:previous_session_info_constants::kPreviousSessionInfoTabCount];
  [NSUserDefaults.standardUserDefaults synchronize];
}

- (void)updateCurrentSessionInactiveTabCount:(NSInteger)count {
  [NSUserDefaults.standardUserDefaults
      setInteger:count
          forKey:previous_session_info_constants::
                     kPreviousSessionInfoInactiveTabCount];
  [NSUserDefaults.standardUserDefaults synchronize];
}

- (void)updateCurrentSessionOTRTabCount:(NSInteger)count {
  [NSUserDefaults.standardUserDefaults
      setInteger:count
          forKey:previous_session_info_constants::
                     kPreviousSessionInfoOTRTabCount];
  [NSUserDefaults.standardUserDefaults synchronize];
}

- (void)setBreadcrumbsLog:(NSString*)breadcrumbs {
  gSharedInstance.breadcrumbs = breadcrumbs;
}

- (void)setReportParameterValue:(NSString*)value forKey:(NSString*)key {
  if (![NSThread isMainThread]) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self setReportParameterValue:value forKey:key];
    });
    return;
  }
  DCHECK([NSThread isMainThread]);
  // Previously this logic would read and write an NSDictionary, but it lead to
  // crashes within the NSUserDefaults logic. Instead, write a separate defaults
  // entry for each key.
  [NSUserDefaults.standardUserDefaults setObject:value
                                          forKey:ReportParamKey(key)];
  [NSUserDefaults.standardUserDefaults synchronize];
}

- (void)setReportParameterURL:(const GURL&)URL forKey:(NSString*)key {
  // Store only URL origin (not whole URL spec) as requested by Privacy Team.
  [self
      setReportParameterValue:base::SysUTF8ToNSString(
                                  URL.DeprecatedGetOriginAsURL().spec().c_str())
                       forKey:key];
}

- (void)removeReportParameterForKey:(NSString*)key {
  if (![NSThread isMainThread]) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self removeReportParameterForKey:key];
    });
    return;
  }
  DCHECK([NSThread isMainThread]);
  [NSUserDefaults.standardUserDefaults removeObjectForKey:ReportParamKey(key)];
  [NSUserDefaults.standardUserDefaults synchronize];
}

@end
