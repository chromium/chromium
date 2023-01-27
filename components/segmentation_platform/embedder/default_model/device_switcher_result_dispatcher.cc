// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"

#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/result.h"

namespace segmentation_platform {

namespace {

const char kDeviceSwitcherUserSegmentPrefKey[] =
    "segmentation_platform.device_switcher_util";

}

const char kDeviceSwitcherFieldTrialName[] = "Segmentation_DeviceSwitcher";

DeviceSwitcherResultDispatcher::DeviceSwitcherResultDispatcher(
    SegmentationPlatformService* segmentation_service,
    PrefService* prefs,
    FieldTrialRegister* field_trial_register)
    : segmentation_service_(segmentation_service),
      prefs_(prefs),
      field_trial_register_(field_trial_register) {
  absl::optional<ClassificationResult> result = ReadResultFromPref();
  if (result && field_trial_register_) {
    field_trial_register_->RegisterFieldTrial(kDeviceSwitcherFieldTrialName,
                                              result->ordered_labels[0]);
  } else if (!result) {
    PredictionOptions options;
    options.on_demand_execution = true;
    segmentation_service_->GetClassificationResult(
        kDeviceSwitcherKey, options, nullptr,
        base::BindOnce(&DeviceSwitcherResultDispatcher::OnGotResult,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

DeviceSwitcherResultDispatcher::~DeviceSwitcherResultDispatcher() = default;

void DeviceSwitcherResultDispatcher::GetClassificationResult(
    ClassificationResultCallback callback) {
  absl::optional<ClassificationResult> result = ReadResultFromPref();
  if (result) {
    std::move(callback).Run(std::move(*result));
    return;
  }

  // This class does not support waiting for multiple requests.
  DCHECK(waiting_callback_.is_null());

  if (initialized_) {
    // If client calls after result is already sent out, return with failure.
    std::move(callback).Run(ClassificationResult(PredictionStatus::kFailed));
    return;
  }
  waiting_callback_ = std::move(callback);
}

// static
void DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDeviceSwitcherUserSegmentPrefKey);
}

void DeviceSwitcherResultDispatcher::OnGotResult(
    const ClassificationResult& result) {
  SaveResultToPref(result);
  initialized_ = true;
  field_trial_register_->RegisterFieldTrial(kDeviceSwitcherFieldTrialName,
                                            result.ordered_labels[0]);
  if (!waiting_callback_.is_null()) {
    std::move(waiting_callback_).Run(result);
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

absl::optional<ClassificationResult>
DeviceSwitcherResultDispatcher::ReadResultFromPref() {
  ClassificationResult result(PredictionStatus::kNotReady);
  const base::Value::Dict& dictionary =
      prefs_->GetDict(kDeviceSwitcherUserSegmentPrefKey);
  const base::Value* value = dictionary.Find("result");
  if (!value) {
    return absl::nullopt;
  }
  const base::Value::Dict& segmentation_result = value->GetDict();
  const base::Value::List* labels_value =
      segmentation_result.FindList("labels");
  if (!labels_value) {
    return absl::nullopt;
  }
  for (const auto& label : *labels_value) {
    result.ordered_labels.push_back(label.GetString());
  }
  result.status = PredictionStatus::kSucceeded;
  return result;
}

}  // namespace segmentation_platform