// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_IOS_PREF_OBSERVER_BRIDGE_H_
#define COMPONENTS_PREFS_IOS_PREF_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include <string>

class PrefChangeRegistrar;

@protocol PrefObserverDelegate
- (void)onPreferenceChanged:(const std::string&)preferenceName;
@end

class PrefObserverBridge {
 public:
  explicit PrefObserverBridge(id<PrefObserverDelegate> delegate);
  virtual ~PrefObserverBridge();

  virtual void ObserveChangesForPreference(const std::string& pref_name,
                                           PrefChangeRegistrar* registrar);

 private:
  virtual void OnPreferenceChanged(const std::string& pref_name);

  __weak id<PrefObserverDelegate> delegate_ = nil;
};

#endif  // COMPONENTS_PREFS_IOS_PREF_OBSERVER_BRIDGE_H_
