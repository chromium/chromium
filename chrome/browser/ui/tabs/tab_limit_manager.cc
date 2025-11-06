// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_limit_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace tab_limit_manager {

int CountRegularTabs(Profile* profile) {
  if (!profile) {
    return 0;
  }

  // Get the original profile (not incognito)
  Profile* original_profile = profile->GetOriginalProfile();

  int count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() == original_profile &&
        !browser->profile()->IsOffTheRecord()) {
      count += browser->tab_strip_model()->count();
    }
  }

  return count;
}

int CountIncognitoTabs(Profile* profile) {
  if (!profile) {
    return 0;
  }

  // Get the original profile to find associated incognito profiles
  Profile* original_profile = profile->GetOriginalProfile();

  int count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() == original_profile &&
        browser->profile()->IsOffTheRecord()) {
      count += browser->tab_strip_model()->count();
    }
  }

  return count;
}

bool CanAddNewTab(Profile* profile, bool is_incognito) {
  if (!profile) {
    return false;
  }

  if (is_incognito) {
    int incognito_count = CountIncognitoTabs(profile);
    return incognito_count < kMaxIncognitoTabs;
  } else {
    int regular_count = CountRegularTabs(profile);
    return regular_count < kMaxRegularTabs;
  }
}

}  // namespace tab_limit_manager
