// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_request_headers.h"

namespace {

// Returns true if the Data Reduction Proxy is forced to be enabled from the
// command line.
bool ShouldForceEnableDataReductionProxy() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);
}

// Key of the UMA DataReductionProxy.StartupState histogram.
const char kUMAProxyStartupStateHistogram[] = "DataReductionProxy.StartupState";

void RecordSettingsEnabledState(
    data_reduction_proxy::DataReductionSettingsEnabledAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      "DataReductionProxy.EnabledState", action,
      data_reduction_proxy::DATA_REDUCTION_SETTINGS_ACTION_BOUNDARY);
}

// Record the number of days since data reduction proxy was enabled by the
// user.
void RecordDaysSinceEnabledMetric(int days_since_enabled) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("DataReductionProxy.DaysSinceEnabled",
                              days_since_enabled, 0, 365 * 10, 100);
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxySettings::DataReductionProxySettings(
    bool is_off_the_record_profile)
    : unreachable_(false),
      prefs_(nullptr),
      clock_(base::DefaultClock::GetInstance()),
      is_off_the_record_profile_(is_off_the_record_profile) {
  DCHECK(!is_off_the_record_profile_);
}

DataReductionProxySettings::~DataReductionProxySettings() = default;

void DataReductionProxySettings::InitDataReductionProxySettings(
    PrefService* prefs,
    std::unique_ptr<DataReductionProxyService> data_reduction_proxy_service) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs);
  DCHECK(data_reduction_proxy_service);
  prefs_ = prefs;
  data_reduction_proxy_service_ = std::move(data_reduction_proxy_service);
  RecordDataReductionInit();

  registrar_.Init(prefs_);
  registrar_.Add(
      prefs::kDataSaverEnabled,
      base::BindRepeating(&DataReductionProxySettings::OnProxyEnabledPrefChange,
                          base::Unretained(this)));
}

void DataReductionProxySettings::SetCallbackToRegisterSyntheticFieldTrial(
    const SyntheticFieldTrialRegistrationCallback&
        on_data_reduction_proxy_enabled) {
  register_synthetic_field_trial_ = on_data_reduction_proxy_enabled;
  RegisterDataReductionProxyFieldTrial();
}

// static
bool DataReductionProxySettings::IsDataSaverEnabledByUser(
    bool is_off_the_record_profile,
    PrefService* prefs) {
  if (is_off_the_record_profile)
    return false;
  if (ShouldForceEnableDataReductionProxy())
    return true;

#if BUILDFLAG(IS_ANDROID)
  return prefs && prefs->GetBoolean(prefs::kDataSaverEnabled);
#else
  return false;
#endif
}

// static
void DataReductionProxySettings::SetDataSaverEnabledForTesting(
    PrefService* prefs,
    bool enabled) {
  // Set the command line so that |IsDataSaverEnabledByUser| returns as expected
  // on all platforms.
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (enabled) {
    cmd->AppendSwitch(switches::kEnableDataReductionProxy);
  } else {
    cmd->RemoveSwitch(switches::kEnableDataReductionProxy);
  }

  // Set the pref so that all the pref change callbacks run.
  prefs->SetBoolean(prefs::kDataSaverEnabled, enabled);
}

bool DataReductionProxySettings::IsDataReductionProxyEnabled() const {
  return IsDataSaverEnabledByUser(is_off_the_record_profile_,
                                  GetOriginalProfilePrefs());
}

bool DataReductionProxySettings::CanUseDataReductionProxy(
    const GURL& url) const {
  return url.is_valid() && url.scheme() == url::kHttpScheme &&
         IsDataReductionProxyEnabled();
}

bool DataReductionProxySettings::IsDataReductionProxyManaged() {
  const PrefService::Preference* pref =
      GetOriginalProfilePrefs()->FindPreference(prefs::kDataSaverEnabled);
  return pref && pref->IsManaged();
}

void DataReductionProxySettings::SetDataReductionProxyEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (GetOriginalProfilePrefs()->GetBoolean(prefs::kDataSaverEnabled) !=
      enabled) {
    GetOriginalProfilePrefs()->SetBoolean(prefs::kDataSaverEnabled, enabled);
    OnProxyEnabledPrefChange();
  }
}

void DataReductionProxySettings::SetUnreachable(bool unreachable) {
  unreachable_ = unreachable;
}

bool DataReductionProxySettings::IsDataReductionProxyUnreachable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return unreachable_;
}

PrefService* DataReductionProxySettings::GetOriginalProfilePrefs() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return prefs_;
}

base::Time DataReductionProxySettings::GetLastEnabledTime() const {
  PrefService* prefs = GetOriginalProfilePrefs();
  int64_t last_enabled_time =
      prefs->GetInt64(prefs::kDataReductionProxyLastEnabledTime);
  if (last_enabled_time <= 0)
    return base::Time();
  return base::Time::FromInternalValue(last_enabled_time);
}

void DataReductionProxySettings::RegisterDataReductionProxyFieldTrial() {
  register_synthetic_field_trial_.Run(
      "SyntheticDataReductionProxySetting",
      IsDataReductionProxyEnabled() ? "Enabled" : "Disabled");
}

void DataReductionProxySettings::OnProxyEnabledPrefChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!register_synthetic_field_trial_.is_null()) {
    RegisterDataReductionProxyFieldTrial();
  }
  MaybeActivateDataReductionProxy(false);

  bool enabled = IsDataReductionProxyEnabled();
  for (auto& observer : observers_)
    observer.OnDataSaverEnabledChanged(enabled);
}

void DataReductionProxySettings::MaybeActivateDataReductionProxy(
    bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PrefService* prefs = GetOriginalProfilePrefs();
  // Do nothing if prefs have not been initialized. This allows unit testing
  // of profile related code without having to initialize data reduction proxy
  // related prefs.
  if (!prefs)
    return;

  bool enabled = IsDataSaverEnabledByUser(is_off_the_record_profile_, prefs);

  if (enabled && at_startup) {
    const auto last_enabled_time = GetLastEnabledTime();
    if (!last_enabled_time.is_null()) {
      // Record the metric only if the time when data reduction proxy was
      // enabled is available.
      RecordDaysSinceEnabledMetric(
          (clock_->Now() - last_enabled_time).InDays());
    }
  }

  if (enabled &&
      !prefs->GetBoolean(prefs::kDataReductionProxyWasEnabledBefore)) {
    prefs->SetBoolean(prefs::kDataReductionProxyWasEnabledBefore, true);
  }
  if (!at_startup) {
    if (IsDataReductionProxyEnabled()) {
      RecordSettingsEnabledState(DATA_REDUCTION_SETTINGS_ACTION_OFF_TO_ON);

      // Data reduction proxy has been enabled by the user. Record the number of
      // days since the data reduction proxy has been enabled as zero, and
      // store the current time in the pref.
      prefs->SetInt64(prefs::kDataReductionProxyLastEnabledTime,
                      clock_->Now().ToInternalValue());
      RecordDaysSinceEnabledMetric(0);
    } else {
      RecordSettingsEnabledState(DATA_REDUCTION_SETTINGS_ACTION_ON_TO_OFF);
    }
  }
}

void DataReductionProxySettings::AddDataReductionProxySettingsObserver(
    DataReductionProxySettingsObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void DataReductionProxySettings::RemoveDataReductionProxySettingsObserver(
    DataReductionProxySettingsObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

// Metrics methods
void DataReductionProxySettings::RecordDataReductionInit() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  RecordStartupState(IsDataReductionProxyEnabled() ? PROXY_ENABLED
                                                   : PROXY_DISABLED);
}

void DataReductionProxySettings::RecordStartupState(
    ProxyStartupState state) const {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyStartupStateHistogram, state,
                            PROXY_STARTUP_STATE_COUNT);
}

}  // namespace data_reduction_proxy
