// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALL_PROMPT_PREFS_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALL_PROMPT_PREFS_H_

#include "base/time/time.h"

class PrefRegistrySimple;
class PrefService;

namespace webapps {

class InstallPromptPrefs {
 public:
  InstallPromptPrefs() = delete;
  ~InstallPromptPrefs() = delete;

  InstallPromptPrefs(const InstallPromptPrefs&) = delete;
  InstallPromptPrefs& operator=(const InstallPromptPrefs&) = delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  static void RecordInstallPromptDismissed(PrefService* pref_service,
                                           base::Time time);
  static void RecordInstallPromptIgnored(PrefService* pref_service,
                                         base::Time time);
  static void RecordInstallPromptClicked(PrefService* pref_service);

  static bool IsPromptDismissedConsecutivelyRecently(PrefService* pref_service,
                                                     base::Time now);
  static bool IsPromptIgnoredConsecutivelyRecently(PrefService* pref_service,
                                                   base::Time now);
};
}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALL_PROMPT_PREFS_H_
