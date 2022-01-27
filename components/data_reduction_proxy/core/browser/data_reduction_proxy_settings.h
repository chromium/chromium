// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

class PrefService;

namespace base {
class Clock;
}

namespace data_reduction_proxy {

class DataReductionProxyService;

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
  // Notifies when Data Saver is enabled or disabled.
  virtual void OnDataSaverEnabledChanged(bool enabled) {}
};

// Central point for configuring the data reduction proxy.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
class DataReductionProxySettings {
 public:
  using SyntheticFieldTrialRegistrationCallback =
      base::RepeatingCallback<bool(base::StringPiece, base::StringPiece)>;

  explicit DataReductionProxySettings(bool is_off_the_record_profile);

  DataReductionProxySettings(const DataReductionProxySettings&) = delete;
  DataReductionProxySettings& operator=(const DataReductionProxySettings&) =
      delete;

  virtual ~DataReductionProxySettings();

  // Initializes the Data Reduction Proxy with the profile prefs. The caller
  // must ensure that all parameters remain alive for the lifetime of the
  // |DataReductionProxySettings| instance.
  void InitDataReductionProxySettings(
      PrefService* prefs,
      std::unique_ptr<DataReductionProxyService> data_reduction_proxy_service);

  // Sets the |register_synthetic_field_trial_| callback and runs to register
  // the DataReductionProxyEnabled synthetic field trial.
  void SetCallbackToRegisterSyntheticFieldTrial(
      const SyntheticFieldTrialRegistrationCallback&
          on_data_reduction_proxy_enabled);

  // Returns true if the Data Saver feature is enabled by the user on Android.
  // This checks only the Data Saver prefs on Android or forcing flag on any
  // platform. Does not check any holdback experiments. Note that this may be
  // different from the value of |IsDataReductionProxyEnabled|.
  static bool IsDataSaverEnabledByUser(bool is_off_the_record_profile,
                                       PrefService* prefs);

  // Enables or disables Data Saver, regardless of platform.
  static void SetDataSaverEnabledForTesting(PrefService* prefs, bool enabled);

  // Returns true if the Data Reduction HTTP Proxy is enabled. Note that this
  // may be different from the value of |IsDataSaverEnabledByUser|.
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

  // Records that the data reduction proxy is unreachable or not.
  void SetUnreachable(bool unreachable);

  // Returns whether the data reduction proxy is unreachable. Returns true
  // if no request has successfully completed through proxy, even though atleast
  // some of them should have.
  bool IsDataReductionProxyUnreachable();

  // Configures data reduction proxy. |at_startup| is true when this method is
  // called in response to creating or loading a new profile.
  void MaybeActivateDataReductionProxy(bool at_startup);

  // Returns the time LiteMode was last enabled. This is reset whenever LiteMode
  // is disabled and re-enabled from settings. Null time is returned when
  // LiteMode has never been enabled.
  base::Time GetLastEnabledTime() const;

  // Adds an observer that is notified every time the proxy request headers
  // change.
  void AddDataReductionProxySettingsObserver(
      DataReductionProxySettingsObserver* observer);

  // Removes an observer that is notified every time the proxy request headers
  // change.
  void RemoveDataReductionProxySettingsObserver(
      DataReductionProxySettingsObserver* observer);

  DataReductionProxyService* data_reduction_proxy_service() {
    return data_reduction_proxy_service_.get();
  }

 protected:
  void InitPrefMembers();

  // Virtualized for unit test support.
  virtual PrefService* GetOriginalProfilePrefs() const;

  // Metrics method. Subclasses should override if they wish to provide
  // alternatives.
  virtual void RecordDataReductionInit() const;

  // Virtualized for mocking. Records UMA specifying whether the proxy was
  // enabled or disabled at startup.
  virtual void RecordStartupState(
      data_reduction_proxy::ProxyStartupState state) const;

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
                           TestSettingsEnabledStateHistograms);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestDaysSinceEnabled);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestDaysSinceEnabledWithTestClock);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestDaysSinceEnabledExistingUser);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestDaysSinceSavingsCleared);

  // Registers the trial "SyntheticDataReductionProxySetting" with the group
  // "Enabled" or "Disabled". Indicates whether the proxy is turned on or not.
  void RegisterDataReductionProxyFieldTrial();

  void OnProxyEnabledPrefChange();

  bool unreachable_;

  std::unique_ptr<DataReductionProxyService> data_reduction_proxy_service_;

  raw_ptr<PrefService> prefs_;

  PrefChangeRegistrar registrar_;

  SyntheticFieldTrialRegistrationCallback register_synthetic_field_trial_;

  // Should not be null.
  raw_ptr<base::Clock> clock_;

  // Observers to notify when the proxy request headers change or |this| is
  // initialized.
  base::ObserverList<DataReductionProxySettingsObserver>::Unchecked observers_;

  // True if |this| was constructed for an off-the-record profile.
  const bool is_off_the_record_profile_;

  base::ThreadChecker thread_checker_;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_
