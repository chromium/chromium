// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_isolation/features.h"

#include "build/build_config.h"

namespace site_isolation {
namespace features {

// Controls a mode for dynamically process-isolating sites where the user has
// entered a password.  This is intended to be used primarily when full site
// isolation is turned off.  To check whether this mode is enabled, use
// SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled() rather than
// checking the feature directly, since that decision is influenced by other
// factors as well.
const base::Feature kSiteIsolationForPasswordSites {
  "site-isolation-for-password-sites",
// Enabled by default on Android; see https://crbug.com/849815.  Note that this
// should not affect Android Webview, which does not include this code.
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Controls a mode for dynamically process-isolating sites where the user has
// logged in via OAuth.  These sites are determined by runtime heuristics.
//
// This is intended to be used primarily when full site isolation is turned
// off.  To check whether this mode is enabled, use
// SiteIsolationPolicy::IsIsolationForOAuthSitesEnabled() rather than
// checking the feature directly, since that decision is influenced by other
// factors as well.
//
// This feature does not affect Android Webview, which does not include this
// code.
const base::Feature kSiteIsolationForOAuthSites{
    "SiteIsolationForOAuthSites", base::FEATURE_DISABLED_BY_DEFAULT};

// kSitePerProcessOnlyForHighMemoryClients is checked before kSitePerProcess,
// and (if enabled) can restrict if kSitePerProcess feature is checked at all -
// no check will be made on devices with low memory (these devices will have no
// Site Isolation via kSitePerProcess trials and won't activate either the
// control or the experiment group).  The threshold for what is considered a
// "low memory" device is set (in MB) via a field trial param with the name
// defined below ("site-per-process-low-memory-cutoff-mb") and compared against
// base::SysInfo::AmountOfPhysicalMemoryMB().
const base::Feature kSitePerProcessOnlyForHighMemoryClients{
    "site-per-process-only-for-high-memory-clients",
    base::FEATURE_DISABLED_BY_DEFAULT};
const char kSitePerProcessOnlyForHighMemoryClientsParamName[] =
    "site-per-process-low-memory-cutoff-mb";

}  // namespace features
}  // namespace site_isolation
