// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_PREFS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_PREFS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

class PrefRegistrySimple;

namespace enterprise_connectors {

// Pref that maps to the "OnFileAttachedEnterpriseConnector" policy.
extern const char kOnFileAttachedPref[];

// Pref that maps to the "OnFileDownloadedEnterpriseConnector" policy.
extern const char kOnFileDownloadedPref[];

// Pref that maps to the "OnBulkDataEntryEnterpriseConnector" policy.
extern const char kOnBulkDataEntryPref[];

// Pref that maps to the "OnPrintEnterpriseConnector" policy.
extern const char kOnPrintPref[];

#if BUILDFLAG(IS_CHROMEOS)
// Pref that maps to the "OnFileTransferEnterpriseConnector" policy.
extern const char kOnFileTransferPref[];
#endif

// Pref that maps to the "OnSecurityEventEnterpriseConnector" policy.
extern const char kOnSecurityEventPref[];

// Pref that maps to the "EnterpriseRealTimeUrlCheckMode" policy.
// The "safebrowsing" prefix is kept for backward compatibility as this constant
// used to be in a SB file.
inline constexpr char kEnterpriseRealTimeUrlCheckMode[] =
    "safebrowsing.enterprise_real_time_url_check_mode";

// Prefs that map to the scope of each policy using a
// EnterpriseConnectorsPolicyHandler.
extern const char kOnFileAttachedScopePref[];
extern const char kOnFileDownloadedScopePref[];
extern const char kOnBulkDataEntryScopePref[];
extern const char kOnPrintScopePref[];
#if BUILDFLAG(IS_CHROMEOS)
extern const char kOnFileTransferScopePref[];
#endif
extern const char kOnSecurityEventScopePref[];
inline constexpr char kEnterpriseRealTimeUrlCheckScope[] =
    "safebrowsing.enterprise_real_time_url_check_scope";

extern const char kLatestCrashReportCreationTime[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_PREFS_H_
