// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service_observer.h"
#include "components/prefs/pref_member.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class Clock;
}

namespace data_reduction_proxy {

class DataReductionProxyConfig;
class DataReductionProxyIOData;
class DataReductionProxyService;
class DataReductionProxyCompressionStats;

// Values of the UMA DataReductionProxy.StartupState histogram.
// This enum must remain synchronized with DataReductionProxyStartupState
// in metrics/histograms/histograms.xml.
enum ProxyStartupState {
  PROXY_NOT_AVAILABLE = 0,
  PROXY_DISABLED,
  PROXY_ENABLED,
  PROXY_STARTUP_STATE_COUNT,
};

// Values of the UMA DataReductionProxy.EnabledState histogram.
// This enum must remain synchronized with DataReductionProxyEnabledState
// in metrics/histograms/histograms.xml.
enum DataReductionSettingsEnabledAction {
  DATA_REDUCTION_SETTINGS_ACTION_OFF_TO_ON = 0,
  DATA_REDUCTION_SETTINGS_ACTION_ON_TO_OFF,
  DATA_REDUCTION_SETTINGS_ACTION_BOUNDARY,
};

// Classes may derive from |DataReductionProxySettingsObserver| and register as
// an observer of |DataReductionProxySettings| to get notified when the proxy
// request headers change or when the DRPSettings class is initialized.
class DataReductionProxySettingsObserver {
 public:
  // Notifies when the proxy server request header change.
  virtual void OnProxyRequestHeadersChanged(
      const net::HttpRequestHeaders& headers) = 0;

  // Notifies when |DataReductionProxySettings::InitDataReductionProxySettings|
  // is finished.
  virtual void OnSettingsInitialized() = 0;
};

// Central point for configuring the data reduction proxy.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
class DataReductionProxySettings : public DataReductionProxyServiceObserver {
 public:
  using SyntheticFieldTrialRegistrationCallback =
      base::Callback<bool(base::StringPiece, base::StringPiece)>;

  DataReductionProxySettings();
  virtual ~DataReductionProxySettings();

  // Initializes the Data Reduction Proxy with the name of the preference that
  // controls enabling it, profile prefs and a |DataReductionProxyIOData|. The
  // caller must ensure that all parameters remain alive for the lifetime of
  // the |DataReductionProxySettings| instance.
  void InitDataReductionProxySettings(
      const std::string& data_reduction_proxy_enabled_pref_name,
      PrefService* prefs,
      DataReductionProxyIOData* io_data,
      std::unique_ptr<DataReductionProxyService> data_reduction_proxy_service);

  // Sets the |register_synthetic_field_trial_| callback and runs to register
  // the DataReductionProxyEnabled and the DataReductionProxyLoFiEnabled
  // synthetic field trial.
  void SetCallbackToRegisterSyntheticFieldTrial(
      const SyntheticFieldTrialRegistrationCallback&
          on_data_reduction_proxy_enabled);

  // Returns true if the proxy is enabled.
  bool IsDataReductionProxyEnabled() const;

  // Returns true if the proxy can be used for the given url. This method does
  // not take into account the proxy config or proxy retry list, so it can
  // return true even when the proxy will not be used. Specifically, if
  // another proxy configuration overrides use of data reduction proxy, or
  // if data reduction proxy is in proxy retry list, then data reduction proxy
  // will not be used, but this method will still return true. If this method
  // returns false, then we are guaranteed that data reduction proxy will not be
  // used.
  bool CanUseDataReductionProxy(const GURL& url) const;

  // Returns true if the proxy is managed by an adminstrator's policy.
  bool IsDataReductionProxyManaged();

  // Enables or disables the data reduction proxy.
  void SetDataReductionProxyEnabled(bool enabled);

  // Returns the time in microseconds that the last update was made to the
  // daily original and received content lengths.
  int64_t GetDataReductionLastUpdateTime();

  // Clears all data saving statistics for the given |reason|.
  void ClearDataSavingStatistics(DataReductionProxySavingsClearedReason reason);

  // Returns the difference between the total original size of all HTTP content
  // received from the network and the actual size of the HTTP content received.
  int64_t GetTotalHttpContentLengthSaved();

  // Returns aggregate received and original content lengths over the specified
  // number of days, as well as the time these stats were last updated.
  void GetContentLengths(unsigned int days,
                         int64_t* original_content_length,
                         int64_t* received_content_length,
                         int64_t* last_update_time);

  // Records that the data reduction proxy is unreachable or not.
  void SetUnreachable(bool unreachable);

  // Returns whether the data reduction proxy is unreachable. Returns true
  // if no request has successfully completed through proxy, even though atleast
  // some of them should have.
  bool IsDataReductionProxyUnreachable();

  // When triggering previews, prevent long term black list rules.
  virtual void SetIgnoreLongTermBlackListRules(
      bool ignore_long_term_black_list_rules) {}

  ContentLengthList GetDailyContentLengths(const char* pref_name);

  // Configures data reduction proxy. |at_startup| is true when this method is
  // called in response to creating or loading a new profile.
  void MaybeActivateDataReductionProxy(bool at_startup);

  // Sets the headers to use for requests to the compression server.
  void SetProxyRequestHeaders(const net::HttpRequestHeaders& headers);

  void SetConfiguredProxies(const net::ProxyList& proxies);

  // Returns headers to use for requests to the compression server.
  const net::HttpRequestHeaders& GetProxyRequestHeaders() const;

  // Adds an observer that is notified every time the proxy request headers
  // change.
  void AddDataReductionProxySettingsObserver(
      DataReductionProxySettingsObserver* observer);

  // Removes an observer that is notified every time the proxy request headers
  // change.
  void RemoveDataReductionProxySettingsObserver(
      DataReductionProxySettingsObserver* observer);

  // Sets a config client that can be used to update Data Reduction Proxy
  // settings when the network service is enabled.
  void SetCustomProxyConfigClient(
      network::mojom::CustomProxyConfigClientPtrInfo proxy_config_client);

  DataReductionProxyService* data_reduction_proxy_service() {
    return data_reduction_proxy_service_.get();
  }

  // Returns the |DataReductionProxyConfig| being used. May be null if
  // InitDataReductionProxySettings has not been called.
  DataReductionProxyConfig* Config() const {
    return config_;
  }

  // Permits changing the underlying |DataReductionProxyConfig| without running
  // the initialization loop.
  void ResetConfigForTest(DataReductionProxyConfig* config) {
    config_ = config;
  }

 protected:
  void InitPrefMembers();

  // Virtualized for unit test support.
  virtual PrefService* GetOriginalProfilePrefs();

  // Metrics method. Subclasses should override if they wish to provide
  // alternatives.
  virtual void RecordDataReductionInit() const;

  // Virtualized for mocking. Records UMA specifying whether the proxy was
  // enabled or disabled at startup.
  virtual void RecordStartupState(
      data_reduction_proxy::ProxyStartupState state) const;

  // Checks whether |proxy_server| is a valid configured proxy.
  bool IsConfiguredDataReductionProxy(
      const net::ProxyServer& proxy_server) const;

 private:
  friend class DataReductionProxySettingsTestBase;
  friend class DataReductionProxySettingsTest;
  friend class DataReductionProxyTestContext;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestResetDataReductionStatistics);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestIsProxyEnabledOrManaged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestCanUseDataReductionProxy);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest, TestContentLengths);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestGetDailyContentLengths);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestMaybeActivateDataReductionProxy);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestOnProxyEnabledPrefChange);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestInitDataReductionProxyOn);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestInitDataReductionProxyOff);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           CheckInitMetricsWhenNotAllowed);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestLoFiImplicitOptOutClicksPerSession);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestLoFiImplicitOptOutConsecutiveSessions);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestLoFiImplicitOptOutHistograms);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestLoFiSessionStateHistograms);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestSettingsEnabledStateHistograms);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestDaysSinceEnabled);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestDaysSinceEnabledWithTestClock);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestDaysSinceEnabledExistingUser);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestDaysSinceSavingsCleared);

  // Override of DataReductionProxyService::Observer.
  void OnServiceInitialized() override;

  // Registers the trial "SyntheticDataReductionProxySetting" with the group
  // "Enabled" or "Disabled". Indicates whether the proxy is turned on or not.
  void RegisterDataReductionProxyFieldTrial();

  void OnProxyEnabledPrefChange();

  // Records data savings percentage histogram at chrome startup, for users who
  // have browsed a reasonable amount. Positive and negative savings are
  // recorded in a separate histogram.
  void RecordStartupSavings() const;

  void ResetDataReductionStatistics();

  // Update IO thread objects in response to UI thread changes.
  void UpdateIOData(bool at_startup);

  // For tests.
  void set_data_reduction_proxy_enabled_pref_name_for_test(
      const std::string& data_reduction_proxy_enabled_pref_name) {
    data_reduction_proxy_enabled_pref_name_ =
        data_reduction_proxy_enabled_pref_name;
  }

  bool unreachable_;

  // A call to MaybeActivateDataReductionProxy may take place before the
  // |data_reduction_proxy_service_| has received a DataReductionProxyIOData
  // pointer. In that case, the operation against the IO objects will not
  // succeed and |deferred_initialization_| will be set to true. When
  // OnServiceInitialized is called, if |deferred_initialization_| is true,
  // IO object calls will be performed at that time.
  bool deferred_initialization_;

  // The number of requests to reload the page with images from the Lo-Fi
  // UI until Lo-Fi is disabled for the remainder of the session.
  int lo_fi_user_requests_for_images_per_session_;

  // The number of consecutive sessions where Lo-Fi was disabled for
  // Lo-Fi to be disabled until the next implicit opt out epoch, which may be in
  // a later session, or never.
  int lo_fi_consecutive_session_disables_;

  BooleanPrefMember spdy_proxy_auth_enabled_;

  std::unique_ptr<DataReductionProxyService> data_reduction_proxy_service_;

  // The name of the preference that controls enabling and disabling the Data
  // Reduction Proxy.
  std::string data_reduction_proxy_enabled_pref_name_;

  PrefService* prefs_;

  // The caller must ensure that the |config_| outlives this instance.
  DataReductionProxyConfig* config_;

  SyntheticFieldTrialRegistrationCallback register_synthetic_field_trial_;

  // Should not be null.
  base::Clock* clock_;

  // Observers to notify when the proxy request headers change or |this| is
  // initialized.
  base::ObserverList<DataReductionProxySettingsObserver>::Unchecked observers_;

  // The headers to use for requests to the proxy server.
  net::HttpRequestHeaders proxy_request_headers_;

  net::ProxyList configured_proxies_;

  network::mojom::CustomProxyConfigClientPtrInfo proxy_config_client_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxySettings);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_
