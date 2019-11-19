// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Safe Browsing utility functions.

#ifndef COMPONENTS_SAFE_BROWSING_COMMON_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_COMMON_UTILS_H_

#include "base/time/time.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "url/gurl.h"

namespace policy {
class BrowserPolicyConnector;
}  // namespace policy

namespace base {
class TimeDelta;
}  // namespace base

class PrefService;

namespace safe_browsing {

// Shorten URL by replacing its contents with its SHA256 hash if it has data
// scheme.
std::string ShortURLForReporting(const GURL& url);

// Gets the |ProfileManagementStatus| for the current machine. The method
// currently works only on Windows and ChromeOS. The |bpc| parameter is used
// only on ChromeOS, and may be |nullptr|.
ChromeUserPopulation::ProfileManagementStatus GetProfileManagementStatus(
    const policy::BrowserPolicyConnector* bpc);

// Util for storing a future alarm time in a pref. |delay| is how much time into
// the future the alarm is set for. Calling GetDelayFromPref() later will return
// a shorter delay, or 0 if it's unset or passed..
void SetDelayInPref(PrefService* prefs,
                    const char* pref_name,
                    const base::TimeDelta& delay);
base::TimeDelta GetDelayFromPref(PrefService* prefs, const char* pref_name);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_COMMON_UTILS_H_
