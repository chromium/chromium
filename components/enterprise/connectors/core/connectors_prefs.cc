// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_prefs.h"

#include <algorithm>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/enterprise/device_trust/prefs.h"
#include "components/prefs/pref_registry_simple.h"

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#include "components/enterprise/client_certificates/core/prefs.h"
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

namespace enterprise_connectors {

// Profile Prefs
const char kOnFileAttachedPref[] = "enterprise_connectors.on_file_attached";

const char kOnFileDownloadedPref[] = "enterprise_connectors.on_file_downloaded";

const char kOnBulkDataEntryPref[] = "enterprise_connectors.on_bulk_data_entry";

const char kOnPrintPref[] = "enterprise_connectors.on_print";

#if BUILDFLAG(IS_CHROMEOS)
const char kOnFileTransferPref[] = "enterprise_connectors.on_file_transfer";
#endif

const char kOnSecurityEventPref[] = "enterprise_connectors.on_security_event";

const char kOnFileAttachedScopePref[] =
    "enterprise_connectors.scope.on_file_attached";
const char kOnFileDownloadedScopePref[] =
    "enterprise_connectors.scope.on_file_downloaded";
const char kOnBulkDataEntryScopePref[] =
    "enterprise_connectors.scope.on_bulk_data_entry";
const char kOnPrintScopePref[] = "enterprise_connectors.scope.on_print";
#if BUILDFLAG(IS_CHROMEOS)
const char kOnFileTransferScopePref[] =
    "enterprise_connectors.scope.on_file_transfer";
#endif
const char kOnSecurityEventScopePref[] =
    "enterprise_connectors.scope.on_security_event";

// Local State Prefs
const char kLatestCrashReportCreationTime[] =
    "enterprise_connectors.latest_crash_report_creation_time";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kEnterpriseRealTimeUrlCheckMode,
                                REAL_TIME_CHECK_DISABLED);
  registry->RegisterIntegerPref(kEnterpriseRealTimeUrlCheckScope, 0);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  registry->RegisterListPref(kOnFileAttachedPref);
  registry->RegisterListPref(kOnFileDownloadedPref);
  registry->RegisterListPref(kOnBulkDataEntryPref);
  registry->RegisterListPref(kOnPrintPref);
#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterListPref(kOnFileTransferPref);
#endif
  registry->RegisterListPref(kOnSecurityEventPref);
  registry->RegisterIntegerPref(kOnFileAttachedScopePref, 0);
  registry->RegisterIntegerPref(kOnFileDownloadedScopePref, 0);
  registry->RegisterIntegerPref(kOnBulkDataEntryScopePref, 0);
  registry->RegisterIntegerPref(kOnPrintScopePref, 0);
#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterIntegerPref(kOnFileTransferScopePref, 0);
#endif
  registry->RegisterIntegerPref(kOnSecurityEventScopePref, 0);
  RegisterDeviceTrustConnectorProfilePrefs(registry);

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
  client_certificates::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(kLatestCrashReportCreationTime, 0);
}

}  // namespace enterprise_connectors
