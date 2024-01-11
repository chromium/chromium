// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/policy/mac/managed_preference_policy_manager_impl.h"

#include <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/enterprise_util.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/policy/manager.h"

namespace {
// Constants for managed preference policy keys.
NSString* kGlobalPolicyKey = @"global";
NSString* kUpdateDefaultKey = @"UpdateDefault";
NSString* kDownloadPreferenceKey = @"DownloadPreference";
NSString* kUpdatesSuppressedStartHourKey = @"UpdatesSuppressedStartHour";
NSString* kUpdatesSuppressedStartMinuteKey = @"UpdatesSuppressedStartMin";
NSString* kUpdatesSuppressedDurationMinuteKey = @"UpdatesSuppressedDurationMin";
NSString* kTargetChannelKey = @"TargetChannel";
NSString* kTargetVersionPrefixKey = @"TargetVersionPrefix";
NSString* kRollbackToTargetVersionKey = @"RollbackToTargetVersion";
}  // namespace

namespace updater {

// Extracts an integer value from a NSString or NSNumber. Returns kPolicyNotSet
// for all unexpected cases.
int ReadPolicyInteger(id value) {
  if (!value) {
    return kPolicyNotSet;
  }

  NSInteger result = kPolicyNotSet;
  if ([value isKindOfClass:[NSString class]]) {
    NSScanner* scanner = [NSScanner scannerWithString:(NSString*)value];
    if (![scanner scanInteger:&result]) {
      return kPolicyNotSet;
    }
  } else if ([value isKindOfClass:[NSNumber class]]) {
    result = [(NSNumber*)value intValue];
  }

  return static_cast<int>(result);
}

// For historical reasons, "update" policy has different enum values in Manage
// Preferences from the Device Management. This function converts the former
// to latter.
// +----------------+---------------------+--------------------+
// | Update policy  | Managed Preferences |  Device Management |
// +----------------+---------------------+--------------------+
// | Enabled        |          0          |         1          |
// +----------------+---------------------+--------------------+
// | Automatic only |          1          |         3          |
// +----------------+---------------------+--------------------+
// | Manual only    |          2          |         2          |
// +----------------+---------------------+--------------------+
// | Disabled       |          3          |         0          |
// +----------------+---------------------+--------------------+
// | Machine only   |          4          |         4          |
// +----------------+---------------------+--------------------+
int TranslateUpdatePolicyValue(int update_policy_from_managed_preferences) {
  switch (update_policy_from_managed_preferences) {
    case 0:
      return kPolicyEnabled;
    case 1:
      return kPolicyAutomaticUpdatesOnly;
    case 2:
      return kPolicyManualUpdatesOnly;
    case 3:
      return kPolicyDisabled;
    case 4:
      return kPolicyEnabledMachineOnly;
    default:
      return kPolicyNotSet;
  }
}

}  // namespace updater

/// Class that manages Mac global-level policies.
@interface CRUManagedPreferenceGlobalPolicySettings : NSObject {
  NSString* __strong _downloadPreference;
};

@property(nonatomic, readonly) int lastCheckPeriodMinutes;
@property(nonatomic, readonly) int defaultUpdatePolicy;
@property(nonatomic, readonly, nullable) NSString* downloadPreference;
@property(nonatomic, readonly, nullable) NSString* proxyMode;
@property(nonatomic, readonly, nullable) NSString* proxyServer;
@property(nonatomic, readonly, nullable) NSString* proxyPacURL;
@property(nonatomic, readonly)
    updater::UpdatesSuppressedTimes updatesSuppressed;

@end

@implementation CRUManagedPreferenceGlobalPolicySettings

@synthesize defaultUpdatePolicy = _defaultUpdatePolicy;
@synthesize updatesSuppressed = _updatesSuppressed;

- (instancetype)initWithDictionary:(CRUAppPolicyDictionary*)policyDict {
  if (([super init])) {
    _downloadPreference =
        base::apple::ObjCCast<NSString>(policyDict[kDownloadPreferenceKey]);
    _defaultUpdatePolicy = updater::TranslateUpdatePolicyValue(
        updater::ReadPolicyInteger(policyDict[kUpdateDefaultKey]));
    _updatesSuppressed.start_hour_ =
        updater::ReadPolicyInteger(policyDict[kUpdatesSuppressedStartHourKey]);
    _updatesSuppressed.start_minute_ = updater::ReadPolicyInteger(
        policyDict[kUpdatesSuppressedStartMinuteKey]);
    _updatesSuppressed.duration_minute_ = updater::ReadPolicyInteger(
        policyDict[kUpdatesSuppressedDurationMinuteKey]);
  }
  return self;
}

- (int)lastCheckPeriodMinutes {
  // LastCheckPeriodMinutes is not supported in Managed Preference policy.
  return updater::kPolicyNotSet;
}

- (NSString*)downloadPreference {
  if (_downloadPreference) {
    return _downloadPreference;
  } else {
    return nil;
  }
}

- (NSString*)proxyMode {
  return nil;  //  ProxyMode is not supported in Managed Preference policy.
}

- (NSString*)proxyServer {
  return nil;  // ProxyServer is not supported in Managed Preference policy.
}

- (NSString*)proxyPacURL {
  return nil;  //  ProxyPacURL is not supported in Managed Preference policy.
}

@end

/// Class that manages policies for a single App.
@interface CRUManagedPreferenceAppPolicySettings : NSObject {
  NSString* __strong _targetChannel;
  NSString* __strong _targetVersionPrefix;
}

@property(nonatomic, readonly) int updatePolicy;
@property(nonatomic, readonly) int rollbackToTargetVersion;
@property(nonatomic, readonly, nullable) NSString* targetChannel;
@property(nonatomic, readonly, nullable) NSString* targetVersionPrefix;

@end

@implementation CRUManagedPreferenceAppPolicySettings

@synthesize updatePolicy = _updatePolicy;
@synthesize rollbackToTargetVersion = _rollbackToTargetVersion;

- (instancetype)initWithDictionary:(CRUAppPolicyDictionary*)policyDict {
  if (([super init])) {
    _updatePolicy = updater::TranslateUpdatePolicyValue(
        updater::ReadPolicyInteger(policyDict[kUpdateDefaultKey]));
    _targetChannel =
        base::apple::ObjCCast<NSString>(policyDict[kTargetChannelKey]);
    _targetVersionPrefix =
        base::apple::ObjCCast<NSString>(policyDict[kTargetVersionPrefixKey]);
    _rollbackToTargetVersion =
        updater::ReadPolicyInteger(policyDict[kRollbackToTargetVersionKey]);
  }

  return self;
}

- (NSString*)targetChannel {
  if (_targetChannel) {
    return [NSString stringWithString:_targetChannel];
  } else {
    return nil;
  }
}

- (NSString*)targetVersionPrefix {
  if (_targetVersionPrefix) {
    return [NSString stringWithString:_targetVersionPrefix];
  } else {
    return nil;
  }
}

@end

@implementation CRUManagedPreferencePolicyManager {
  CRUManagedPreferenceGlobalPolicySettings* __strong _globalPolicy;
  NSMutableDictionary<NSString*, CRUManagedPreferenceAppPolicySettings*>*
      __strong _appPolicies;
}

@synthesize hasActivePolicy = _hasActivePolicy;

- (instancetype)initWithDictionary:(CRUUpdatePolicyDictionary*)policies {
  if (([super init])) {
    _hasActivePolicy = policies.count > 0;

    // Always create a global policy instance for default values.
    _globalPolicy = [[CRUManagedPreferenceGlobalPolicySettings alloc]
        initWithDictionary:nil];

    _appPolicies = [[NSMutableDictionary alloc] init];
    for (NSString* __strong appid in policies.allKeys) {
      if (![policies[appid] isKindOfClass:[CRUAppPolicyDictionary class]]) {
        continue;
      }

      CRUAppPolicyDictionary* policyDict = policies[appid];
      appid = appid.lowercaseString;
      if ([appid isEqualToString:kGlobalPolicyKey]) {
        _globalPolicy = [[CRUManagedPreferenceGlobalPolicySettings alloc]
            initWithDictionary:policyDict];
      } else {
        CRUManagedPreferenceAppPolicySettings* appSettings =
            [[CRUManagedPreferenceAppPolicySettings alloc]
                initWithDictionary:policyDict];
        [_appPolicies setObject:appSettings forKey:appid];
      }
    }
  }
  return self;
}

- (NSString*)source {
  return [NSString
      stringWithUTF8String:updater::kSourceManagedPreferencePolicyManager];
}

- (NSString*)downloadPreference {
  return [_globalPolicy downloadPreference];
}

- (NSString*)proxyMode {
  return [_globalPolicy proxyMode];
}

- (NSString*)proxyServer {
  return [_globalPolicy proxyServer];
}

- (NSString*)proxyPacURL {
  return [_globalPolicy proxyPacURL];
}

- (int)lastCheckPeriodMinutes {
  return [_globalPolicy lastCheckPeriodMinutes];
}

- (int)defaultUpdatePolicy {
  return [_globalPolicy defaultUpdatePolicy];
}

- (updater::UpdatesSuppressedTimes)updatesSuppressed {
  return [_globalPolicy updatesSuppressed];
}

- (int)appUpdatePolicy:(NSString*)appid {
  appid = appid.lowercaseString;
  if (![_appPolicies objectForKey:appid]) {
    return updater::kPolicyNotSet;
  }
  return [_appPolicies objectForKey:appid].updatePolicy;
}

- (NSString*)targetChannel:(NSString*)appid {
  appid = appid.lowercaseString;
  return [_appPolicies objectForKey:appid].targetChannel;
}

- (NSString*)targetVersionPrefix:(NSString*)appid {
  appid = appid.lowercaseString;
  return [_appPolicies objectForKey:appid].targetVersionPrefix;
}

- (int)rollbackToTargetVersion:(NSString*)appid {
  appid = appid.lowercaseString;
  if (![_appPolicies objectForKey:appid]) {
    return updater::kPolicyNotSet;
  }
  return [_appPolicies objectForKey:appid].rollbackToTargetVersion;
}

- (NSArray<NSString*>*)appsWithPolicy {
  return [_appPolicies allKeys];
}

@end
