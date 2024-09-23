// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/telemetry/fake_probe_service.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

}  // namespace

FakeProbeService::FakeProbeService() : receiver_(this) {}

FakeProbeService::~FakeProbeService() = default;

void FakeProbeService::BindPendingReceiver(
    mojo::PendingReceiver<crosapi::TelemetryProbeService> receiver) {
  receiver_.Bind(std::move(receiver));
}

mojo::PendingRemote<crosapi::TelemetryProbeService>
FakeProbeService::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeProbeService::ProbeTelemetryInfo(
    const std::vector<crosapi::ProbeCategoryEnum>& categories,
    ProbeTelemetryInfoCallback callback) {
  probe_telemetry_info_call_count_ += 1;
  probe_telemetry_info_requested_categories_.clear();
  probe_telemetry_info_requested_categories_.insert(
      probe_telemetry_info_requested_categories_.end(), categories.begin(),
      categories.end());

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), telem_info_.Clone()));
}

void FakeProbeService::GetOemData(GetOemDataCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), oem_data_.Clone()));
}

const std::vector<crosapi::ProbeCategoryEnum>&
FakeProbeService::GetLastRequestedCategories() {
  return probe_telemetry_info_requested_categories_;
}

void FakeProbeService::SetProbeTelemetryInfoResponse(
    crosapi::ProbeTelemetryInfoPtr response_info) {
  telem_info_ = std::move(response_info);
}

void FakeProbeService::SetOemDataResponse(crosapi::ProbeOemDataPtr oem_data) {
  oem_data_ = std::move(oem_data);
}

int FakeProbeService::GetProbeTelemetryInfoCallCount() {
  return probe_telemetry_info_call_count_;
}

}  // namespace chromeos
