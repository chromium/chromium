// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_UTILS_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_UTILS_H_

#include "base/no_destructor.h"

class Profile;

class TabOrganizationUtils {
 public:
  static TabOrganizationUtils* GetInstance();

  TabOrganizationUtils(const TabOrganizationUtils&) = delete;
  TabOrganizationUtils& operator=(const TabOrganizationUtils&) = delete;

  void SetIgnoreOptGuideForTesting(bool ignore) {
    ignore_opt_guide_for_testing_ = ignore;
  }

  // Returns true if the tab organization feature should be treated as enabled
  // for the given profile.
  bool IsEnabled(Profile* profile);

 protected:
  TabOrganizationUtils();
  ~TabOrganizationUtils();

 private:
  friend base::NoDestructor<TabOrganizationUtils>;
  bool ignore_opt_guide_for_testing_ = false;
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZATION_TAB_ORGANIZATION_UTILS_H_
