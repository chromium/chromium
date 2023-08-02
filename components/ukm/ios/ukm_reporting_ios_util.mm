// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ios/ukm_reporting_ios_util.h"

#import <UIKit/UIKit.h>

#include "base/metrics/histogram_functions.h"

namespace {
// Key in NSUserDefaults to store the count of "UKM.LogSize.OnSuccess" records.
NSString* LogSizeOnSuccessCounterKey = @"IOSUKMLogSizeOnSuccessCounter";
}

void RecordAndResetUkmLogSizeOnSuccessCounter() {
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
  NSInteger counter = [defaults integerForKey:LogSizeOnSuccessCounterKey];
  base::UmaHistogramCounts10000("UKM.IOSLog.OnSuccess", counter);
  [defaults removeObjectForKey:LogSizeOnSuccessCounterKey];
}

void IncrementUkmLogSizeOnSuccessCounter() {
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
  NSInteger counter = [defaults integerForKey:LogSizeOnSuccessCounterKey] + 1;
  [defaults setInteger:counter forKey:LogSizeOnSuccessCounterKey];
}
