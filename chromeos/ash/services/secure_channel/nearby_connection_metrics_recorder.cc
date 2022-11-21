// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/nearby_connection_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"

namespace ash::secure_channel {

namespace {

static constexpr base::TimeDelta kEffectiveSuccessRateTimeout =
    base::Minutes(1);

void RecordEffectiveConnectionResult(bool success) {
  base::UmaHistogramBoolean(
      "MultiDevice.SecureChannel.Nearby.EffectiveConnectionResult", success);
}

}  // namespace

NearbyConnectionMetricsRecorder::NearbyConnectionMetricsRecorder() = default;

NearbyConnectionMetricsRecorder::~NearbyConnectionMetricsRecorder() = default;

void NearbyConnectionMetricsRecorder::HandleConnectionSuccess(
    const DeviceIdPair& device_id_pair) {
  RecordEffectiveConnectionResult(/*success=*/true);

  // If there was a previous unsuccessful attempt, clear the unsuccessful
  // timestamp since there was a successful retry.
  id_pair_to_first_unsuccessful_timestamp_map_[device_id_pair] = base::Time();
}

void NearbyConnectionMetricsRecorder::HandleConnectionFailure(
    const DeviceIdPair& device_id_pair) {
  base::Time& first_unsuccessful_time =
      id_pair_to_first_unsuccessful_timestamp_map_[device_id_pair];

  // If the first unsuccessful timstamp for this ID pair is already set, return
  // early to ensure that we do not log multiple repeated failures.
  if (!first_unsuccessful_time.is_null())
    return;

  // Set the current time as the first unsuccessful timestamp.
  first_unsuccessful_time = base::Time::Now();

  // Start a timeout period; if no successful attempts occur before this period,
  // we'll log a failure.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NearbyConnectionMetricsRecorder::OnTimeout,
                     weak_ptr_factory_.GetWeakPtr(), device_id_pair),
      kEffectiveSuccessRateTimeout);
}

void NearbyConnectionMetricsRecorder::OnTimeout(
    const DeviceIdPair& device_id_pair) {
  const base::Time& first_unsuccessful_time =
      id_pair_to_first_unsuccessful_timestamp_map_[device_id_pair];

  // There was a successful retry during the timeout period; do not log a
  // failure result.
  if (first_unsuccessful_time.is_null() ||
      base::Time::Now() - first_unsuccessful_time <
          kEffectiveSuccessRateTimeout) {
    return;
  }

  RecordEffectiveConnectionResult(/*success=*/false);
}

}  // namespace ash::secure_channel
