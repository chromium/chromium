// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/fake_statistics_provider.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/system/statistics_provider.h"

namespace ash::system {

FakeStatisticsProvider::FakeStatisticsProvider() = default;

FakeStatisticsProvider::~FakeStatisticsProvider() = default;

void FakeStatisticsProvider::StartLoadingMachineStatistics(
    bool load_oem_manifest) {
}

void FakeStatisticsProvider::ScheduleOnMachineStatisticsLoaded(
    base::OnceClosure callback) {
  // No load is required for FakeStatisticsProvider.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

std::optional<std::string_view> FakeStatisticsProvider::GetMachineStatistic(
    std::string_view name) {
  const auto match = machine_statistics_.find(name);
  if (match == machine_statistics_.end())
    return std::nullopt;

  return std::string_view(match->second);
}

FakeStatisticsProvider::FlagValue FakeStatisticsProvider::GetMachineFlag(
    std::string_view name) {
  const auto match = machine_flags_.find(name);
  if (match == machine_flags_.end())
    return FlagValue::kUnset;

  return match->second ? FlagValue::kTrue : FlagValue::kFalse;
}

void FakeStatisticsProvider::Shutdown() {
}

bool FakeStatisticsProvider::IsRunningOnVm() {
  return GetMachineStatistic(kIsVmKey) == kIsVmValueTrue;
}

bool FakeStatisticsProvider::IsCrosDebugMode() {
  return GetMachineStatistic(kIsCrosDebugKey) == kIsCrosDebugValueTrue;
}

StatisticsProvider::VpdStatus FakeStatisticsProvider::GetVpdStatus() const {
  return vpd_status_;
}

void FakeStatisticsProvider::SetMachineStatistic(const std::string& key,
                                                 const std::string& value) {
  machine_statistics_[key] = value;
}

void FakeStatisticsProvider::ClearMachineStatistic(std::string_view key) {
  machine_statistics_.erase(key);
}

void FakeStatisticsProvider::ClearAllMachineStatistics() {
  machine_statistics_.clear();
}

void FakeStatisticsProvider::SetMachineFlag(const std::string& key,
                                            bool value) {
  machine_flags_[key] = value;
}

void FakeStatisticsProvider::ClearMachineFlag(std::string_view key) {
  machine_flags_.erase(key);
}

void FakeStatisticsProvider::SetVpdStatus(VpdStatus new_status) {
  vpd_status_ = new_status;
}

ScopedFakeStatisticsProvider::ScopedFakeStatisticsProvider() {
  StatisticsProvider::SetTestProvider(this);
}

ScopedFakeStatisticsProvider::~ScopedFakeStatisticsProvider() {
  StatisticsProvider::SetTestProvider(nullptr);
}

}  // namespace ash::system
