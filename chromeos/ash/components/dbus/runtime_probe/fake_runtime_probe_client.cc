// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/runtime_probe/fake_runtime_probe_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

namespace {
constexpr int kLiveValuesLimit = 42;
}

FakeRuntimeProbeClient::FakeRuntimeProbeClient() {}

FakeRuntimeProbeClient::~FakeRuntimeProbeClient() = default;

void FakeRuntimeProbeClient::ProbeCategories(
    const runtime_probe::ProbeRequest& request,
    RuntimeProbeCallback callback) {
  runtime_probe::ProbeResult result;

  for (const auto& category : request.categories()) {
    if (category == runtime_probe::ProbeRequest::battery) {
      runtime_probe::Battery* battery = result.add_battery();
      battery->set_name("generic");
      auto* battery_fields = battery->mutable_values();
      battery_fields->set_manufacturer("111-22-");
      battery_fields->set_model_name("AA12345");
      battery_fields->set_serial_number("0123");
      // Charge in 1e-6 Ah
      battery_fields->set_charge_full_design(5 * 1000 * 1000);
      battery_fields->set_charge_full(4 * 1000 * 1000);
      battery_fields->set_charge_now(4 * 1000 * 1000 - 10000 * live_offset_);
      // Voltage in 1e-6 V
      battery_fields->set_voltage_now(10 * 1000 * 1000 - 5000 * live_offset_);
      battery_fields->set_cycle_count_smart(1);
      battery_fields->set_status_smart(224);
      // Temperature in 0.1 deg K
      battery_fields->set_temperature_smart(3030 + live_offset_);
      continue;
    }

    if (category == runtime_probe::ProbeRequest::storage) {
      runtime_probe::Storage* storage = result.add_storage();
      storage->set_name("generic");
      auto* storage_fields = storage->mutable_values();
      storage_fields->set_sectors(123456789);
      storage_fields->set_type("MMC");
      // Manufacturer ID
      storage_fields->set_mmc_manfid(123);
      storage_fields->set_mmc_name("aB1cD>");
      // Product revision
      storage_fields->set_mmc_prv(1);
      storage_fields->set_mmc_serial(12345678);
      storage_fields->set_mmc_oemid(123);
    }
  }

  // Change offset, but keep it in reasonably small range.
  live_offset_ = (live_offset_ + 1) % kLiveValuesLimit;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

}  // namespace ash
