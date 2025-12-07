// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/prefs/ios/pref_observer_bridge.h"

#include "base/functional/bind.h"
#include "components/prefs/pref_change_registrar.h"

PrefObserverBridge::PrefObserverBridge(id<PrefObserverDelegate> delegate)
    : delegate_(delegate) {}

PrefObserverBridge::~PrefObserverBridge() = default;

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
