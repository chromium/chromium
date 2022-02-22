// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"

namespace {

// Returns true if the Data Reduction Proxy is forced to be enabled from the
// command line.
bool ShouldForceEnableDataReductionProxy() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxySettings::DataReductionProxySettings(
    bool is_off_the_record_profile)
    : is_off_the_record_profile_(is_off_the_record_profile) {
  DCHECK(!is_off_the_record_profile_);
}

DataReductionProxySettings::~DataReductionProxySettings() = default;

void DataReductionProxySettings::InitDataReductionProxySettings() {}

// static
bool DataReductionProxySettings::IsDataSaverEnabledByUser(
    bool is_off_the_record_profile) {
  if (is_off_the_record_profile)
    return false;
  if (ShouldForceEnableDataReductionProxy())
    return true;
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

}  // namespace data_reduction_proxy
