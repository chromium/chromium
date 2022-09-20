// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PREFERENCE_MANAGER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PREFERENCE_MANAGER_H_

#include "base/memory/raw_ptr.h"

class PrefService;

namespace autofill_assistant {
// A wrapper around preferences used by Autofill Assistant that encapsulates
// logic for checking combinations of preferences.
class PreferenceManager {
 public:
  explicit PreferenceManager(PrefService* pref_service);
  virtual ~PreferenceManager();

  // Gets and sets whether a user is a first time trigger script user.
  bool GetIsFirstTimeTriggerScriptUser() const;
  void SetIsFirstTimeTriggerScriptUser(bool first_time_user);

 private:
  // The `PrefService` from which to read and write prefs.
  const raw_ptr<PrefService> pref_service_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PREFERENCE_MANAGER_H_
