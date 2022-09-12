// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_FAKE_STATISTICS_PROVIDER_H_
#define CHROMEOS_SYSTEM_FAKE_STATISTICS_PROVIDER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "chromeos/system/statistics_provider.h"

namespace chromeos {
namespace system {

// A fake StatisticsProvider implementation that is useful in tests.
class COMPONENT_EXPORT(CHROMEOS_SYSTEM) FakeStatisticsProvider
    : public StatisticsProvider {
 public:
  FakeStatisticsProvider();

  FakeStatisticsProvider(const FakeStatisticsProvider&) = delete;
  FakeStatisticsProvider& operator=(const FakeStatisticsProvider&) = delete;

  ~FakeStatisticsProvider() override;

  // StatisticsProvider implementation:
  void ScheduleOnMachineStatisticsLoaded(base::OnceClosure callback) override;
  void StartLoadingMachineStatistics(bool load_oem_manifest) override;
  bool GetMachineStatistic(const std::string& name,
                           std::string* result) override;
  bool GetMachineFlag(const std::string& name, bool* result) override;
  void Shutdown() override;
  bool IsRunningOnVm() override;

  void SetMachineStatistic(const std::string& key, const std::string& value);
  void ClearMachineStatistic(const std::string& key);
  void SetMachineFlag(const std::string& key, bool value);
  void ClearMachineFlag(const std::string& key);

 private:
  std::map<std::string, std::string> machine_statistics_;
  std::map<std::string, bool> machine_flags_;
};

// A convenience subclass that automatically registers itself as the test
// StatisticsProvider during construction and cleans up at destruction.
class COMPONENT_EXPORT(CHROMEOS_SYSTEM) ScopedFakeStatisticsProvider
    : public FakeStatisticsProvider {
 public:
  ScopedFakeStatisticsProvider();

  ScopedFakeStatisticsProvider(const ScopedFakeStatisticsProvider&) = delete;
  ScopedFakeStatisticsProvider& operator=(const ScopedFakeStatisticsProvider&) =
      delete;

  ~ScopedFakeStatisticsProvider() override;
};

}  // namespace system
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
namespace system {
using ::chromeos::system::ScopedFakeStatisticsProvider;
}
}  // namespace ash

#endif  // CHROMEOS_SYSTEM_FAKE_STATISTICS_PROVIDER_H_
