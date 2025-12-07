// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_ACTION_PREFS_LISTENER_H_
#define CHROME_BROWSER_UI_BROWSER_ACTION_PREFS_LISTENER_H_

#include "base/memory/raw_ref.h"
#include "components/prefs/pref_change_registrar.h"

class BrowserActions;
class Profile;

class BrowserActionPrefsListener final {
 public:
  BrowserActionPrefsListener(Profile* profile, BrowserActions* browser_actions);
  BrowserActionPrefsListener(const BrowserActionPrefsListener&) = delete;
  BrowserActionPrefsListener& operator=(const BrowserActionPrefsListener&) =
      delete;
  ~BrowserActionPrefsListener();

 private:
  void UpdateActionsForSharingHubPolicy();
  void UpdateQRCodeGeneratorActionEnabledState();

  const raw_ref<Profile> profile_;
  const raw_ref<BrowserActions> browser_actions_;
  PrefChangeRegistrar profile_pref_registrar_;
  PrefChangeRegistrar local_pref_registrar_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_ACTION_PREFS_LISTENER_H_
