// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/prefs.h"

#include <string>

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/buildflags/buildflags.h"

namespace enterprise_reporting {

namespace {
const base::TimeDelta kDefaultReportFrequency = base::Hours(24);
}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  // This is also registered as a Profile pref which will be removed after
  // the migration.
  registry->RegisterBooleanPref(kCloudReportingEnabled, false);
  registry->RegisterTimePref(kLastUploadTimestamp, base::Time());
  registry->RegisterTimePref(kLastUploadSucceededTimestamp, base::Time());
#if !BUILDFLAG(IS_IOS)
  registry->RegisterStringPref(kLastUploadVersion, std::string());
#endif  // !BUILDFLAG(IS_IOS)
  registry->RegisterTimeDeltaPref(kCloudReportingUploadFrequency,
                                  kDefaultReportFrequency);
  registry->RegisterListPref(kSaasUsageDomainUrlsForBrowser);
  registry->RegisterDictionaryPref(kSaasUsageReport);
  registry->RegisterTimePref(kSaasUsageReportLastTriggerTime, base::Time());
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kCloudProfileReportingEnabled, false);
  registry->RegisterTimePref(kLastUploadTimestamp, base::Time());
  registry->RegisterTimePref(kLastUploadSucceededTimestamp, base::Time());
  registry->RegisterTimePref(kLastSignalsUploadAttemptTimestamp, base::Time());
  registry->RegisterTimePref(kLastSignalsUploadSucceededTimestamp,
                             base::Time());
  registry->RegisterStringPref(kLastSignalsUploadSucceededConfig,
                               std::string());
#if !BUILDFLAG(IS_IOS)
  registry->RegisterStringPref(kLastUploadVersion, std::string());
#endif  // !BUILDFLAG(IS_IOS)
  registry->RegisterTimeDeltaPref(kCloudReportingUploadFrequency,
                                  kDefaultReportFrequency);
  registry->RegisterBooleanPref(kUserSecuritySignalsReporting, false);
  registry->RegisterBooleanPref(kUserSecurityAuthenticatedReporting, false);
  registry->RegisterListPref(kSecuritySignalsClientCertificatesSelectors);
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  registry->RegisterBooleanPref(
      enterprise_reporting::kCloudExtensionRequestEnabled, false);
  registry->RegisterDictionaryPref(
      enterprise_reporting::kCloudExtensionRequestIds);
  registry->RegisterBooleanPref(kExtensionDOMActivityLoggingEnabled, false);
  registry->RegisterDictionaryPref(kCloudExtensionRequestUploadedIds);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if !BUILDFLAG(IS_IOS)
  registry->RegisterListPref(kCloudLegacyTechReportAllowlist);
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(kPoliciesEverFetchedWithProfileId, false);
#endif  // BUILDFLAG(IS_IOS)

  registry->RegisterListPref(kSaasUsageDomainUrlsForProfile);
  registry->RegisterDictionaryPref(kSaasUsageReport);
  registry->RegisterTimePref(kSaasUsageReportLastTriggerTime, base::Time());
}

}  // namespace enterprise_reporting
