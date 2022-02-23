// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

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
  explicit DataReductionProxySettings(bool is_off_the_record_profile);

  DataReductionProxySettings(const DataReductionProxySettings&) = delete;
  DataReductionProxySettings& operator=(const DataReductionProxySettings&) =
      delete;

  virtual ~DataReductionProxySettings();

  // Initializes the Data Reduction Proxy with the profile prefs. The caller
  // must ensure that all parameters remain alive for the lifetime of the
  // |DataReductionProxySettings| instance.
  void InitDataReductionProxySettings();

  // Returns true if the Data Saver feature is enabled by the user on Android.
  // This checks only the Data Saver prefs on Android or forcing flag on any
  // platform. Does not check any holdback experiments. Note that this may be
  // different from the value of |IsDataReductionProxyEnabled|.
  static bool IsDataSaverEnabledByUser(bool is_off_the_record_profile);

  // Enables or disables Data Saver, regardless of platform.
  static void SetDataSaverEnabledForTesting(bool enabled);

  // Adds an observer that is notified every time the proxy request headers
  // change.
  void AddDataReductionProxySettingsObserver(
      DataReductionProxySettingsObserver* observer);

  // Removes an observer that is notified every time the proxy request headers
  // change.
  void RemoveDataReductionProxySettingsObserver(
      DataReductionProxySettingsObserver* observer);

 private:
  // Observers to notify when the proxy request headers change or |this| is
  // initialized.
  base::ObserverList<DataReductionProxySettingsObserver>::Unchecked observers_;

  // True if |this| was constructed for an off-the-record profile.
  const bool is_off_the_record_profile_;

  base::ThreadChecker thread_checker_;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_
