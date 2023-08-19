// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include <Foundation/Foundation.h>

#include "base/feature_list.h"

namespace metrics {
namespace {

// Used to enable the workaround for a local state not persisting sometimes.
NSString* const kLastSessionExitedCleanly = @"LastSessionExitedCleanly";
// Because variations are not initialized this early in startup, pair a user
// defaults value with the variations config.
BASE_FEATURE(kUseUserDefaultsForExitedCleanlyBeacon,
             "UseUserDefaultsForExitedCleanlyBeaconEnabler",
             base::FEATURE_DISABLED_BY_DEFAULT);
NSString* const kUserDefaultsFeatureFlagForExitedCleanlyBeacon =
    @"UserDefaultsFeatureFlagForExitedCleanlyBeacon";

}

// static
void CleanExitBeacon::SetUserDefaultsBeacon(bool exited_cleanly) {
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
  [defaults setBool:exited_cleanly forKey:kLastSessionExitedCleanly];
  [defaults synchronize];
}

// static
bool CleanExitBeacon::ShouldUseUserDefaultsBeacon() {
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
  return [defaults boolForKey:kUserDefaultsFeatureFlagForExitedCleanlyBeacon];
}

// static
void CleanExitBeacon::SyncUseUserDefaultsBeacon() {
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
  [defaults setBool:base::FeatureList::IsEnabled(
                        kUseUserDefaultsForExitedCleanlyBeacon)
             forKey:kUserDefaultsFeatureFlagForExitedCleanlyBeacon];
  [defaults synchronize];
}

// static
bool CleanExitBeacon::HasUserDefaultsBeacon() {
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
  return [defaults objectForKey:kLastSessionExitedCleanly] != nil;
}

// static
bool CleanExitBeacon::GetUserDefaultsBeacon() {
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
  return [defaults boolForKey:kLastSessionExitedCleanly];
}

// static
void CleanExitBeacon::ResetUserDefaultsBeacon() {
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
  [defaults removeObjectForKey:kLastSessionExitedCleanly];
  [defaults synchronize];
}

}  // namespace metrics
