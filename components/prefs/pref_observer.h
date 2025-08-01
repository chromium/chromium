// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_OBSERVER_H_
#define COMPONENTS_PREFS_PREF_OBSERVER_H_

#include <string_view>

#include "base/observer_list_types.h"

class PrefService;

// Used internally to the Prefs subsystem to pass preference change
// notifications between PrefService, PrefNotifierImpl, PrefMemberBase
// and PrefChangeRegistrar.
class PrefObserver : public base::CheckedObserver {
 public:
  // Invoked before the destruction of the PrefService. The PrefObserver
  // must unsubscribe from the PrefService (and must no longer reference
  // the PrefService).
  virtual void OnServiceDestroyed(PrefService* service) = 0;

  // Invoked when the value of the preference named `pref_name` changes.
  virtual void OnPreferenceChanged(PrefService* service,
                                   std::string_view pref_name) = 0;
};

#endif  // COMPONENTS_PREFS_PREF_OBSERVER_H_
