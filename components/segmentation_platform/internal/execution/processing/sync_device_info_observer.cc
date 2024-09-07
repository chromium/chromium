// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/processing/sync_device_info_observer.h"

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace segmentation_platform::processing {

using OsType = syncer::DeviceInfo::OsType;
using FormFactor = syncer ::DeviceInfo::FormFactor;

namespace {

constexpr int kActiveDaysThresholdForMetrics = 14;
constexpr int kActiveDayThresholdForInputDelegate = 60;

#define AS_FLOAT_VAL(x) ProcessedValue(static_cast<float>(x))

base::TimeDelta GetActivePeriodForMetrics() {
  TRACE_EVENT0("ui", "sync_device_info_observer.cc::GetActivePeriodForMetrics");
  return base::Days(base::GetFieldTrialParamByFeatureAsInt(
      kSegmentationDeviceCountByOsType, "active_days_threshold",
      kActiveDaysThresholdForMetrics));
}

base::TimeDelta Age(base::Time last_update, base::Time now) {
  // Don't allow negative age for things somehow updated in the future.
  return std::max(base::TimeDelta(), now - last_update);
}

// Determines if a device with |last_update| timestamp should be considered
// active, given the current time.
bool IsDeviceActive(base::Time last_update,
                    base::Time now,
                    std::optional<base::TimeDelta> active_threshold) {
  TRACE_EVENT0("ui", "sync_device_info_observer.cc::GetActivePeriodForMetrics");
  base::TimeDelta active_days_threshold =
      active_threshold ? *active_threshold : GetActivePeriodForMetrics();
  return Age(last_update, now) < active_days_threshold;
}

// Keep the following in sync with variants in
// //tools/metrics/histograms/metadata/segmentation_platform/histograms.xml.
const char* ConvertOsTypeToString(OsType os_type) {
  switch (os_type) {
    case OsType::kWindows:
      return "Windows";
    case OsType::kMac:
      return "Mac";
    case OsType::kLinux:
      return "Linux";
    case OsType::kIOS:
      return "iOS";
    case OsType::kAndroid:
      return "Android";
    case OsType::kChromeOsAsh:
      return "ChromeOsAsh";
    case OsType::kChromeOsLacros:
      return "ChromeOsLacros";
    case OsType::kFuchsia:
      return "Fuchsia";
    case OsType::kUnknown:
      return "Unknown";
  }
}

}  // namespace

BASE_FEATURE(kSegmentationDeviceCountByOsType,
             "SegmentationDeviceCountByOsType",
             base::FEATURE_ENABLED_BY_DEFAULT);

SyncDeviceInfoObserver::SyncDeviceInfoObserver(
    syncer::DeviceInfoTracker* device_info_tracker)
    : device_info_tracker_(device_info_tracker) {
  DCHECK(device_info_tracker_);
  device_info_tracker_->AddObserver(this);
}

SyncDeviceInfoObserver::~SyncDeviceInfoObserver() {
  device_info_tracker_->RemoveObserver(this);
}

// Count device by os types and record them in UMA only if not recorded yet.
void SyncDeviceInfoObserver::OnDeviceInfoChange() {
  TRACE_EVENT0("ui", "SyncDeviceInfoObserver::OnDeviceInfoChange");
  if (!device_info_tracker_->IsSyncing() ||
      device_info_status_ == DeviceInfoStatus::INFO_AVAILABLE) {
    return;
  }

  device_info_status_ = DeviceInfoStatus::INFO_AVAILABLE;

  // Run any method calls that were received during initialization.
  while (!pending_actions_.empty()) {
    TRACE_EVENT0("ui", "post_pending_action");
    auto callback = std::move(pending_actions_.front());
    pending_actions_.pop_front();
    device_info_status_ = DeviceInfoStatus::INFO_AVAILABLE;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }

  // Record device count by OS types.
  std::map<OsType, int> count_by_os_type =
      CountActiveDevicesByOsType(GetActivePeriodForMetrics());

  // Record UMA metrics of device counts by OS types.
  // Record 0 when there are no devices associated with one OS type.
  for (int os_type_idx = static_cast<int>(OsType::kUnknown);
       os_type_idx <= static_cast<int>(OsType::kFuchsia); ++os_type_idx) {
    OsType os_type = static_cast<OsType>(os_type_idx);
    int count = count_by_os_type[os_type];
    base::UmaHistogramSparse(
        base::StringPrintf("SegmentationPlatform.DeviceCountByOsType.%s",
                           ConvertOsTypeToString(os_type)),
        std::min(count, 100));
  }
}

std::map<OsType, int> SyncDeviceInfoObserver::CountActiveDevicesByOsType(
    base::TimeDelta active_threshold) const {
  TRACE_EVENT0("ui", "SyncDeviceInfoObserver::CountActiveDevicesByOsType");
  std::map<OsType, int> count_by_os_type;
  const base::Time now = base::Time::Now();
  for (const syncer::DeviceInfo* device_info :
       device_info_tracker_->GetAllChromeDeviceInfo()) {
    if (!IsDeviceActive(device_info->last_updated_timestamp(), now,
                        active_threshold)) {
      continue;
    }

    auto os_type = device_info->os_type();
    count_by_os_type[os_type] += 1;
  }
  return count_by_os_type;
}

void SyncDeviceInfoObserver::Process(
    const proto::CustomInput& input,
    FeatureProcessorState& feature_processor_state,
    ProcessedCallback callback) {
  int wait_for_device_info_in_seconds = 0;

  auto model_input_it =
      input.additional_args().find("wait_for_device_info_in_seconds");
  std::optional<int> wait_from_input;
  if (feature_processor_state.input_context()) {
    auto api_input_it =
        feature_processor_state.input_context()->metadata_args.find(
            "wait_for_device_info_in_seconds");
    if (api_input_it !=
        feature_processor_state.input_context()->metadata_args.end()) {
      CHECK_EQ(api_input_it->second.type, ProcessedValue::Type::INT);
      wait_from_input = api_input_it->second.int_val;
    }
  }
  if (wait_from_input) {
    wait_for_device_info_in_seconds = *wait_from_input;
  } else if (model_input_it != input.additional_args().end()) {
    if (!base::StringToInt(model_input_it->second,
                           &wait_for_device_info_in_seconds)) {
      wait_for_device_info_in_seconds = 0;
    }
  }

  if (wait_for_device_info_in_seconds > 0 &&
      (device_info_status_ == DeviceInfoStatus::TIMEOUT_NOT_POSTED ||
       device_info_status_ == DeviceInfoStatus::TIMEOUT_POSTED_BUT_NOT_HIT)) {
    pending_actions_.push_back(base::BindOnce(
        &SyncDeviceInfoObserver::ReadyToFinishProcessing,
        weak_ptr_factory_.GetWeakPtr(), input,
        feature_processor_state.input_context(), std::move(callback)));

    if (device_info_status_ == DeviceInfoStatus::TIMEOUT_NOT_POSTED) {
      device_info_status_ = DeviceInfoStatus::TIMEOUT_POSTED_BUT_NOT_HIT;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&SyncDeviceInfoObserver::OnTimeout,
                         weak_ptr_factory_.GetWeakPtr()),
          base::Seconds(wait_for_device_info_in_seconds));
    }
  } else {
    ReadyToFinishProcessing(
        input, feature_processor_state.input_context(), std::move(callback),
        device_info_status_ == DeviceInfoStatus::INFO_AVAILABLE);
  }
}

void SyncDeviceInfoObserver::ReadyToFinishProcessing(
    const proto::CustomInput& input,
    scoped_refptr<InputContext> input_context,
    ProcessedCallback callback,
    bool success) {
  if (!success) {
    Tensor inputs(10, ProcessedValue(0.0f));
    inputs[0] = AS_FLOAT_VAL(1);  // failure.
    std::move(callback).Run(/*error=*/false, std::move(inputs));
    return;
  }

  std::optional<base::TimeDelta> active_threshold;
  if (input_context) {
    active_threshold = base::Days(kActiveDayThresholdForInputDelegate);
    auto input_context_iter =
        input_context->metadata_args.find("active_days_limit");
    if (input_context_iter != input_context->metadata_args.end()) {
      const auto& processed_value = input_context_iter->second;
      if (processed_value.type == ProcessedValue::INT) {
        active_threshold = base::Days(processed_value.int_val);
      }
    }
  }
  std::map<
      std::pair<syncer::DeviceInfo::FormFactor, syncer::DeviceInfo::OsType>,
      int>
      device_count_by_type;
  int total_count = 0;
  const base::Time now = base::Time::Now();
  for (const syncer::DeviceInfo* device_info :
       device_info_tracker_->GetAllDeviceInfo()) {
    if (device_info_tracker_->IsRecentLocalCacheGuid(device_info->guid())) {
      continue;
    }

    if (!IsDeviceActive(device_info->last_updated_timestamp(), now,
                        active_threshold)) {
      continue;
    }

    auto os_type = device_info->os_type();
    device_count_by_type[{device_info->form_factor(), os_type}] += 1;
    total_count++;
  }

  Tensor inputs(10, ProcessedValue(0.0f));
  inputs[0] = AS_FLOAT_VAL(0);  // success.
  inputs[1] = AS_FLOAT_VAL(
      (device_count_by_type[{FormFactor::kPhone, OsType::kAndroid}]));
  inputs[2] = AS_FLOAT_VAL(
      (device_count_by_type[{FormFactor::kTablet, OsType::kAndroid}]));
  inputs[3] =
      AS_FLOAT_VAL((device_count_by_type[{FormFactor::kPhone, OsType::kIOS}]));
  inputs[4] =
      AS_FLOAT_VAL((device_count_by_type[{FormFactor::kTablet, OsType::kIOS}]));
  inputs[5] = AS_FLOAT_VAL(
      (device_count_by_type[{FormFactor::kDesktop, OsType::kLinux}]));
  inputs[6] = AS_FLOAT_VAL(
      (device_count_by_type[{FormFactor::kDesktop, OsType::kMac}]));
  inputs[7] = AS_FLOAT_VAL(
      (device_count_by_type[{FormFactor::kDesktop, OsType::kWindows}]));
  inputs[8] = AS_FLOAT_VAL(
      (device_count_by_type[{FormFactor::kDesktop, OsType::kChromeOsLacros}]));
  int known_type_count = 0;
  for (unsigned i = 1; i <= 8; ++i) {
    known_type_count += inputs[i].float_val;
  }
  inputs[9] = AS_FLOAT_VAL(total_count - known_type_count);
  std::move(callback).Run(/*error=*/false, std::move(inputs));
}

void SyncDeviceInfoObserver::OnTimeout() {
  if (device_info_status_ == DeviceInfoStatus::INFO_AVAILABLE) {
    return;
  }

  device_info_status_ = DeviceInfoStatus::INFO_UNAVAILABLE;

  while (!pending_actions_.empty()) {
    auto callback = std::move(pending_actions_.front());
    pending_actions_.pop_front();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
  }
  return;
}

}  // namespace segmentation_platform::processing
