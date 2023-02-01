// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/public/constants.h"
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
    syncer::SyncService* sync_service,
    PrefService* prefs,
    FieldTrialRegister* field_trial_register)
    : segmentation_service_(segmentation_service),
      sync_service_(sync_service),
      prefs_(prefs),
      field_trial_register_(field_trial_register) {
  latest_result_ = ReadResultFromPref();
  RegisterFieldTrials();

  if (sync_service_) {
    has_sync_consent_at_startup_ = sync_service_->HasSyncConsent();
    if (has_sync_consent_at_startup_) {
      sync_consent_timestamp_ = base::Time::Now();
    } else {
      sync_observation_.Observe(sync_service);
    }
  }

  if (!latest_result_ || (has_sync_consent_at_startup_ &&
                          latest_result_->ordered_labels[0] ==
                              DeviceSwitcherModel::kNotSyncedLabel)) {
    RefreshSegmentResult();
  }
}

DeviceSwitcherResultDispatcher::~DeviceSwitcherResultDispatcher() = default;

void DeviceSwitcherResultDispatcher::GetClassificationResult(
    ClassificationResultCallback callback) {
  if (latest_result_) {
    std::move(callback).Run(std::move(*latest_result_));
    return;
  }

  // This class does not support waiting for multiple requests.
  DCHECK(waiting_callback_.is_null());
  waiting_callback_ = std::move(callback);
}

// static
void DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDeviceSwitcherUserSegmentPrefKey);
}

void DeviceSwitcherResultDispatcher::OnStateChanged(syncer::SyncService* sync) {
  DCHECK_EQ(sync_consent_timestamp_, base::Time());
  if (sync->HasSyncConsent()) {
    sync_consent_timestamp_ = base::Time::Now();
    sync_observation_.Reset();

    if (!latest_result_ || latest_result_->ordered_labels[0] ==
                               DeviceSwitcherModel::kNotSyncedLabel) {
      RefreshSegmentResult();
    }
  }
}

void DeviceSwitcherResultDispatcher::OnSyncShutdown(syncer::SyncService* sync) {
  sync_observation_.Reset();
}

void DeviceSwitcherResultDispatcher::RefreshSegmentResult() {
  PredictionOptions options;
  options.on_demand_execution = true;
  segmentation_service_->GetClassificationResult(
      kDeviceSwitcherKey, options, nullptr,
      base::BindOnce(&DeviceSwitcherResultDispatcher::OnGotResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceSwitcherResultDispatcher::OnGotResult(
    const ClassificationResult& result) {
  base::TimeDelta consent_verification_to_result_duration =
      base::Time::Now() - sync_consent_timestamp_;
  SaveResultToPref(result);
  latest_result_ = result;
  RegisterFieldTrials();

  if (has_sync_consent_at_startup_) {
    base::UmaHistogramMediumTimes(
        "SegmentationPlatform.DeviceSwicther.TimeFromStartupToResult",
        consent_verification_to_result_duration);
  } else {
    base::UmaHistogramMediumTimes(
        "SegmentationPlatform.DeviceSwicther.TimeFromConsentToResult",
        consent_verification_to_result_duration);
  }
  if (!waiting_callback_.is_null()) {
    std::move(waiting_callback_).Run(result);
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