// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/prefs/ios/pref_observer_bridge.h"

#include "base/functional/bind.h"
#include "components/prefs/pref_change_registrar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PrefObserverBridge::PrefObserverBridge(id<PrefObserverDelegate> delegate)
    : delegate_(delegate) {}

PrefObserverBridge::~PrefObserverBridge() {}

void PrefObserverBridge::ObserveChangesForPreference(
    const std::string& pref_name,
    PrefChangeRegistrar* registrar) {
  PrefChangeRegistrar::NamedChangeCallback callback = base::BindRepeating(
      &PrefObserverBridge::OnPreferenceChanged, base::Unretained(this));
  registrar->Add(pref_name.c_str(), callback);
}

void PrefObserverBridge::OnPreferenceChanged(const std::string& pref_name) {
  [delegate_ onPreferenceChanged:pref_name];
}
