// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/http/http_status_code.h"
#include "url/url_constants.h"

#if defined(OS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace {

const char kEnabled[] = "Enabled";

const char kExperimentsOption[] = "exp";

bool IsIncludedInFieldTrial(const std::string& name) {
  return base::StartsWith(base::FieldTrialList::FindFullName(name), kEnabled,
                          base::CompareCase::SENSITIVE);
}

bool CanShowAndroidLowMemoryDevicePromo() {
#if defined(OS_ANDROID)
  return base::SysInfo::IsLowEndDevice() &&
         base::FeatureList::IsEnabled(
             data_reduction_proxy::features::
                 kDataReductionProxyLowMemoryDevicePromo);
#endif
  return false;
}

}  // namespace

namespace data_reduction_proxy {
namespace params {

bool IsIncludedInPromoFieldTrial() {
  if (IsIncludedInFieldTrial("DataCompressionProxyPromoVisibility"))
    return true;

  return CanShowAndroidLowMemoryDevicePromo();
}

bool IsIncludedInFREPromoFieldTrial() {
  if (IsIncludedInFieldTrial("DataReductionProxyFREPromo"))
    return true;

  return CanShowAndroidLowMemoryDevicePromo();
}

std::string GetDataSaverServerExperimentsOptionName() {
  return kExperimentsOption;
}

std::string GetDataSaverServerExperiments() {
  const std::string cmd_line_experiment =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          data_reduction_proxy::switches::kDataReductionProxyExperiment);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          data_reduction_proxy::switches::
              kDataReductionProxyServerExperimentsDisabled)) {
    // Both kDataReductionProxyExperiment and
    // kDataReductionProxyServerExperimentsDisabled switches can't be set at the
    // same time.
    DCHECK(cmd_line_experiment.empty());
    return std::string();
  }

  // Experiment set using command line overrides field trial.
  if (!cmd_line_experiment.empty())
    return cmd_line_experiment;

  // First check if the feature is enabled.
  if (!base::FeatureList::IsEnabled(
          features::kDataReductionProxyServerExperiments)) {
    return std::string();
  }
  return base::GetFieldTrialParamValueByFeature(
      features::kDataReductionProxyServerExperiments, kExperimentsOption);
}

}  // namespace params

}  // namespace data_reduction_proxy
