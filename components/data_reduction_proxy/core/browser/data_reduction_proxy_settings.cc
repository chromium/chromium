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
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
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
const char kUMAProxyStartupStateHistogram[] =
    "DataReductionProxy.StartupState";

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
      config_(nullptr),
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
  DCHECK(data_reduction_proxy_service->config());
  prefs_ = prefs;
  config_ = data_reduction_proxy_service->config();
  data_reduction_proxy_service_ = std::move(data_reduction_proxy_service);
  RecordDataReductionInit();

  registrar_.Init(prefs_);
  registrar_.Add(
      prefs::kDataSaverEnabled,
      base::BindRepeating(&DataReductionProxySettings::OnProxyEnabledPrefChange,
                          base::Unretained(this)));

#if defined(OS_ANDROID)
  if (IsDataSaverEnabledByUser(is_off_the_record_profile_, prefs_)) {
    data_reduction_proxy_service_->compression_stats()
        ->SetDataUsageReportingEnabled(true);
  }
#endif  // defined(OS_ANDROID)

  for (auto& observer : observers_)
    observer.OnSettingsInitialized();

  while (!proxy_config_clients_.empty()) {
    data_reduction_proxy_service_->AddCustomProxyConfigClient(
        std::move(proxy_config_clients_.back()));
    proxy_config_clients_.pop_back();
  }
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

#if defined(OS_ANDROID)
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
  if (!params::IsEnabledWithNetworkService()) {
    return false;
  }
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
  DCHECK(data_reduction_proxy_service_->compression_stats());
  if (GetOriginalProfilePrefs()->GetBoolean(prefs::kDataSaverEnabled) !=
      enabled) {
    GetOriginalProfilePrefs()->SetBoolean(prefs::kDataSaverEnabled, enabled);
    OnProxyEnabledPrefChange();
#if defined(OS_ANDROID)
    data_reduction_proxy_service_->compression_stats()
        ->SetDataUsageReportingEnabled(enabled);
#endif  // defined(OS_ANDROID)
  }
}

int64_t DataReductionProxySettings::GetDataReductionLastUpdateTime() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());
  return
      data_reduction_proxy_service_->compression_stats()->GetLastUpdateTime();
}

void DataReductionProxySettings::ClearDataSavingStatistics(
    DataReductionProxySavingsClearedReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());
  data_reduction_proxy_service_->compression_stats()->ClearDataSavingStatistics(
      reason);
}

int64_t DataReductionProxySettings::GetTotalHttpContentLengthSaved() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return data_reduction_proxy_service_->compression_stats()
             ->GetHttpOriginalContentLength() -
         data_reduction_proxy_service_->compression_stats()
             ->GetHttpReceivedContentLength();
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

void DataReductionProxySettings::ResetDataReductionStatistics() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());
  data_reduction_proxy_service_->compression_stats()->ResetStatistics();
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
    // Record the number of days since data reduction proxy has been enabled.
    int64_t last_enabled_time =
        prefs->GetInt64(prefs::kDataReductionProxyLastEnabledTime);
    if (last_enabled_time != 0) {
      // Record the metric only if the time when data reduction proxy was
      // enabled is available.
      RecordDaysSinceEnabledMetric(
          (clock_->Now() - base::Time::FromInternalValue(last_enabled_time))
              .InDays());
    }
  }

  if (enabled &&
      !prefs->GetBoolean(prefs::kDataReductionProxyWasEnabledBefore)) {
    prefs->SetBoolean(prefs::kDataReductionProxyWasEnabledBefore, true);
    ResetDataReductionStatistics();
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

  data_reduction_proxy_service_->SetProxyPrefs(IsDataReductionProxyEnabled(),
                                               at_startup);
}

const net::HttpRequestHeaders&
DataReductionProxySettings::GetProxyRequestHeaders() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return proxy_request_headers_;
}

void DataReductionProxySettings::SetProxyRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  DCHECK(thread_checker_.CalledOnValidThread());
  proxy_request_headers_ = headers;
  for (auto& observer : observers_)
    observer.OnProxyRequestHeadersChanged(headers);
}

bool DataReductionProxySettings::IsConfiguredDataReductionProxy(
    const net::ProxyServer& proxy_server) const {
  if (proxy_server.is_direct() || !proxy_server.is_valid())
    return false;

  net::ProxyList proxies =
      data_reduction_proxy_service_->config()->GetAllConfiguredProxies();
  for (const auto& drp_proxy : proxies.GetAll()) {
    if (drp_proxy.host_port_pair().Equals(proxy_server.host_port_pair()))
      return true;
  }
  return false;
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

void DataReductionProxySettings::AddCustomProxyConfigClient(
    mojo::Remote<network::mojom::CustomProxyConfigClient> proxy_config_client) {
  if (data_reduction_proxy_service_) {
    data_reduction_proxy_service_->AddCustomProxyConfigClient(
        std::move(proxy_config_client));
    return;
  }

  proxy_config_clients_.push_back(std::move(proxy_config_client));
}

// Metrics methods
void DataReductionProxySettings::RecordDataReductionInit() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  RecordStartupState(IsDataReductionProxyEnabled() ? PROXY_ENABLED
                                                   : PROXY_DISABLED);
  RecordStartupSavings();
}

void DataReductionProxySettings::RecordStartupState(
    ProxyStartupState state) const {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyStartupStateHistogram,
                            state,
                            PROXY_STARTUP_STATE_COUNT);
}

void DataReductionProxySettings::RecordStartupSavings() const {
  // Minimum bytes the user should have browsed, for the data savings percent
  // UMA to be recorded at startup.
  const unsigned int kMinOriginalContentLengthBytes =
      10 * 1024 * 1024;  // 10 MB.

  if (!IsDataReductionProxyEnabled())
    return;

  DCHECK(data_reduction_proxy_service_->compression_stats());
  int64_t original_content_length =
      data_reduction_proxy_service_->compression_stats()
          ->GetHttpOriginalContentLength();
  int64_t received_content_length =
      data_reduction_proxy_service_->compression_stats()
          ->GetHttpReceivedContentLength();
  if (original_content_length < kMinOriginalContentLengthBytes)
    return;
  int savings_percent =
      static_cast<int>(((original_content_length - received_content_length) /
                        (float)original_content_length) *
                       100.0);
  if (savings_percent >= 0) {
    UMA_HISTOGRAM_PERCENTAGE("DataReductionProxy.StartupSavingsPercent",
                             savings_percent > 0 ? savings_percent : 0);
  }
  if (savings_percent < 0) {
    UMA_HISTOGRAM_PERCENTAGE("DataReductionProxy.StartupNegativeSavingsPercent",
                             -savings_percent);
  }
}

ContentLengthList
DataReductionProxySettings::GetDailyContentLengths(const char* pref_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());
  return data_reduction_proxy_service_->compression_stats()->
      GetDailyContentLengths(pref_name);
}

void DataReductionProxySettings::GetContentLengths(
    unsigned int days,
    int64_t* original_content_length,
    int64_t* received_content_length,
    int64_t* last_update_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_reduction_proxy_service_->compression_stats());

  data_reduction_proxy_service_->compression_stats()->GetContentLengths(
      days, original_content_length, received_content_length, last_update_time);
}

}  // namespace data_reduction_proxy
