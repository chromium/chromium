// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner_util.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/pref_service.h"

namespace data_reduction_proxy {

DataReductionProxyService::DataReductionProxyService(
    DataReductionProxySettings* settings,
    PrefService* prefs)
    : settings_(settings), prefs_(prefs) {
  DCHECK(settings);
}

DataReductionProxyService::~DataReductionProxyService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DataReductionProxyService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
}

void DataReductionProxyService::SetUnreachable(bool unreachable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  settings_->SetUnreachable(unreachable);
}

void DataReductionProxyService::SetInt64Pref(const std::string& pref_path,
                                             int64_t value) {
  if (prefs_)
    prefs_->SetInt64(pref_path, value);
}

void DataReductionProxyService::SetStringPref(const std::string& pref_path,
                                              const std::string& value) {
  if (prefs_)
    prefs_->SetString(pref_path, value);
}

base::WeakPtr<DataReductionProxyService>
DataReductionProxyService::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

double DataReductionProxyService::GetSaveDataSavingsPercentEstimate(
    const std::string& origin) const {
  return 0;
}

}  // namespace data_reduction_proxy
