// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"

#include <memory>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace data_reduction_proxy {

// Make sure any changes here that have the potential to impact android_webview
// are reflected in RegisterSimpleProfilePrefs.
void RegisterSyncableProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kDataSaverEnabled, false);
  registry->RegisterBooleanPref(prefs::kDataReductionProxyWasEnabledBefore,
                                false);

  registry->RegisterInt64Pref(prefs::kDataReductionProxyLastEnabledTime, 0L);

  registry->RegisterBooleanPref(prefs::kDataUsageReportingEnabled, false);

  registry->RegisterInt64Pref(prefs::kHttpReceivedContentLength, 0);
  registry->RegisterInt64Pref(prefs::kHttpOriginalContentLength, 0);

  registry->RegisterListPref(prefs::kDailyHttpOriginalContentLength);

  registry->RegisterListPref(prefs::kDailyHttpReceivedContentLength);

  registry->RegisterInt64Pref(prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
  registry->RegisterStringPref(prefs::kDataReductionProxyConfig, std::string());
  registry->RegisterInt64Pref(prefs::kDataReductionProxyLastConfigRetrievalTime,
                              0L);
  registry->RegisterDictionaryPref(prefs::kNetworkProperties);

  registry->RegisterIntegerPref(prefs::kThisWeekNumber, false);
  registry->RegisterDictionaryPref(
      prefs::kThisWeekServicesDownstreamBackgroundKB, PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(
      prefs::kThisWeekServicesDownstreamForegroundKB, PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(
      prefs::kLastWeekServicesDownstreamBackgroundKB, PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(
      prefs::kLastWeekServicesDownstreamForegroundKB, PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(
      prefs::kThisWeekUserTrafficContentTypeDownstreamKB,
      PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(
      prefs::kLastWeekUserTrafficContentTypeDownstreamKB,
      PrefRegistry::LOSSY_PREF);
}

void RegisterSimpleProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kDataReductionProxyWasEnabledBefore, false);

  registry->RegisterBooleanPref(prefs::kDataUsageReportingEnabled, false);
  RegisterPrefs(registry);
}

// Add any new data reduction proxy prefs to the |pref_map_| or the
// |list_pref_map_| in Init() of DataReductionProxyCompressionStats.
void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDataReductionProxy, std::string());
  registry->RegisterInt64Pref(prefs::kDataReductionProxyLastEnabledTime, 0L);
  registry->RegisterInt64Pref(prefs::kHttpReceivedContentLength, 0);
  registry->RegisterInt64Pref(
      prefs::kHttpOriginalContentLength, 0);
  registry->RegisterListPref(
      prefs::kDailyHttpOriginalContentLength);
  registry->RegisterListPref(prefs::kDailyHttpReceivedContentLength);
  registry->RegisterInt64Pref(
      prefs::kDailyHttpContentLengthLastUpdateDate, 0L);
  registry->RegisterStringPref(prefs::kDataReductionProxyConfig, std::string());
  registry->RegisterInt64Pref(prefs::kDataReductionProxyLastConfigRetrievalTime,
                              0L);
  registry->RegisterDictionaryPref(prefs::kNetworkProperties);

  registry->RegisterIntegerPref(prefs::kThisWeekNumber, false);
  registry->RegisterDictionaryPref(
      prefs::kThisWeekServicesDownstreamBackgroundKB, PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(
      prefs::kThisWeekServicesDownstreamForegroundKB, PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(
      prefs::kLastWeekServicesDownstreamBackgroundKB, PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(
      prefs::kLastWeekServicesDownstreamForegroundKB, PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(
      prefs::kThisWeekUserTrafficContentTypeDownstreamKB,
      PrefRegistry::LOSSY_PREF);
  registry->RegisterDictionaryPref(
      prefs::kLastWeekUserTrafficContentTypeDownstreamKB,
      PrefRegistry::LOSSY_PREF);
}

}  // namespace data_reduction_proxy
