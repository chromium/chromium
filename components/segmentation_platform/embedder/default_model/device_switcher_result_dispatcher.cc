// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/field_trial_register.h"
#include "components/segmentation_platform/public/result.h"

namespace segmentation_platform {

namespace {

const char kDeviceSwitcherUserSegmentPrefKey[] =
    "segmentation_platform.device_switcher_util";

}

const char kDeviceSwitcherFieldTrialName[] = "Segmentation_DeviceSwitcher";

DeviceSwitcherResultDispatcher::DeviceSwitcherResultDispatcher(
    SegmentationPlatformService* segmentation_service,
    syncer::DeviceInfoTracker* device_info_tracker,
    PrefService* prefs,
    FieldTrialRegister* field_trial_register)
    : segmentation_service_(segmentation_service),
      device_info_tracker_(device_info_tracker),
      prefs_(prefs),
      field_trial_register_(field_trial_register) {
  latest_result_ = ReadResultFromPref();
  RegisterFieldTrials();

  if (!device_info_tracker_->IsSyncing()) {
    device_info_observation_.Observe(device_info_tracker_);
  }

  RefreshSegmentResultIfNeeded();
}

DeviceSwitcherResultDispatcher::~DeviceSwitcherResultDispatcher() = default;

ClassificationResult
DeviceSwitcherResultDispatcher::GetCachedClassificationResult() {
  std::optional<ClassificationResult> result =
      latest_result_ ? latest_result_ : ReadResultFromPref();
  return result.has_value() ? result.value()
                            : ClassificationResult(PredictionStatus::kNotReady);
}

void DeviceSwitcherResultDispatcher::WaitForClassificationResult(
    base::TimeDelta timeout,
    ClassificationResultCallback callback) {
  if (latest_result_) {
    std::move(callback).Run(std::move(*latest_result_));
    return;
  }

  // This class does not support waiting for multiple requests.
  DCHECK(waiting_callback_.is_null());
  waiting_callback_ = std::move(callback);

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DeviceSwitcherResultDispatcher::OnWaitTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      timeout);
}

// static
void DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDeviceSwitcherUserSegmentPrefKey);
}

void DeviceSwitcherResultDispatcher::OnDeviceInfoChange() {
  if (!device_info_tracker_->IsSyncing()) {
    return;
  }
  device_info_observation_.Reset();
  RefreshSegmentResultIfNeeded();
}

void DeviceSwitcherResultDispatcher::RefreshSegmentResultIfNeeded() {
  if (!base::FeatureList::IsEnabled(
          features::kSegmentationPlatformDeviceSwitcher)) {
    return;
  }
  if (latest_result_ && (latest_result_->ordered_labels.empty() ||
                         latest_result_->ordered_labels[0] !=
                             DeviceSwitcherModel::kNotSyncedLabel)) {
    return;
  }
  PredictionOptions options;
  options.on_demand_execution = true;
  segmentation_service_->GetClassificationResult(
      kDeviceSwitcherKey, options, nullptr,
      base::BindOnce(&DeviceSwitcherResultDispatcher::OnGotResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceSwitcherResultDispatcher::OnGotResult(
    const ClassificationResult& result) {
  SaveResultToPref(result);
  latest_result_ = result;
  RegisterFieldTrials();

  if (!waiting_callback_.is_null()) {
    std::move(waiting_callback_).Run(result);
  }
}

void DeviceSwitcherResultDispatcher::OnWaitTimeout() {
  if (!waiting_callback_.is_null()) {
    std::move(waiting_callback_)
        .Run(ClassificationResult(PredictionStatus::kNotReady));
  }
}

void DeviceSwitcherResultDispatcher::RegisterFieldTrials() {
  if (!field_trial_register_ || !latest_result_) {
    return;
  }
  if (latest_result_->status == PredictionStatus::kSucceeded &&
      !latest_result_->ordered_labels.empty()) {
    field_trial_register_->RegisterFieldTrial(
        kDeviceSwitcherFieldTrialName, latest_result_->ordered_labels[0]);
  } else {
    field_trial_register_->RegisterFieldTrial(kDeviceSwitcherFieldTrialName,
                                              "Unselected");
  }
}

void DeviceSwitcherResultDispatcher::SaveResultToPref(
    const ClassificationResult& result) {
  ScopedDictPrefUpdate update(prefs_, kDeviceSwitcherUserSegmentPrefKey);
  base::Value::Dict& dictionary = update.Get();
  if (result.status != PredictionStatus::kSucceeded) {
    dictionary.Remove("result");
    return;
  }
  base::Value::Dict segmentation_result;
  base::Value::List labels;
  for (const auto& label : result.ordered_labels) {
    labels.Append(label);
  }
  segmentation_result.Set("labels", std::move(labels));
  dictionary.Set("result", std::move(segmentation_result));
}

std::optional<ClassificationResult>
DeviceSwitcherResultDispatcher::ReadResultFromPref() const {
  ClassificationResult result(PredictionStatus::kNotReady);
  const base::Value::Dict& dictionary =
      prefs_->GetDict(kDeviceSwitcherUserSegmentPrefKey);
  const base::Value* value = dictionary.Find("result");
  if (!value) {
    return std::nullopt;
  }
  const base::Value::Dict& segmentation_result = value->GetDict();
  const base::Value::List* labels_value =
      segmentation_result.FindList("labels");
  if (!labels_value) {
    return std::nullopt;
  }
  for (const auto& label : *labels_value) {
    result.ordered_labels.push_back(label.GetString());
  }
  result.status = PredictionStatus::kSucceeded;
  return result;
}

}  // namespace segmentation_platform
