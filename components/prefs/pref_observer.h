// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_OBSERVER_H_
#define COMPONENTS_PREFS_PREF_OBSERVER_H_

#include <string_view>

class PrefService;

// Used internally to the Prefs subsystem to pass preference change
// notifications between PrefService, PrefNotifierImpl and
// PrefChangeRegistrar.
class PrefObserver {
 public:
  virtual void OnPreferenceChanged(PrefService* service,
                                   std::string_view pref_name) = 0;
};

#endif  // COMPONENTS_PREFS_PREF_OBSERVER_H_
