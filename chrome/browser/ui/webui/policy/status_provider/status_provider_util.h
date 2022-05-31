// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_STATUS_PROVIDER_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_STATUS_PROVIDER_UTIL_H_

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"

void ExtractDomainFromUsername(base::DictionaryValue* dict);

// Adds a new entry to |dict| with the affiliation status of the user associated
// with |profile|. This method shouldn't be called for device scope status.
void GetUserAffiliationStatus(base::DictionaryValue* dict, Profile* profile);

// MachineStatus box labels itself as `machine policies` on desktop. In the
// domain of mobile devices such as iOS or Android we want to label this box as
// `device policies`. This is a helper function that retrieves the expected
// labelKey
std::string GetMachineStatusLegendKey();

#if BUILDFLAG(IS_CHROMEOS_ASH)
void GetOffHoursStatus(base::DictionaryValue* dict);

// Adds a new entry to |dict| with the enterprise domain manager of the user
// associated with |profile|. This method shouldn't be called for device scope
// status.
void GetUserManager(base::DictionaryValue* dict, Profile* profile);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_STATUS_PROVIDER_STATUS_PROVIDER_UTIL_H_
