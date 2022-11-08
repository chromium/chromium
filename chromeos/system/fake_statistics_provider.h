// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_FAKE_STATISTICS_PROVIDER_H_
#define CHROMEOS_SYSTEM_FAKE_STATISTICS_PROVIDER_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"
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
  absl::optional<base::StringPiece> GetMachineStatistic(
      base::StringPiece name) override;
  FlagValue GetMachineFlag(base::StringPiece name) override;
  void Shutdown() override;
  bool IsRunningOnVm() override;
  VpdStatus GetVpdStatus() const override;

  // TODO(b/213325251): Remove old getters once migration is completed.
  using StatisticsProvider::GetMachineFlag;
  using StatisticsProvider::GetMachineStatistic;

  void SetMachineStatistic(const std::string& key, const std::string& value);
  void ClearMachineStatistic(base::StringPiece key);
  void SetMachineFlag(const std::string& key, bool value);
  void ClearMachineFlag(base::StringPiece key);
  void SetVpdStatus(VpdStatus new_status);

 private:
  base::flat_map<std::string, std::string> machine_statistics_;
  base::flat_map<std::string, bool> machine_flags_;

  VpdStatus vpd_status_{VpdStatus::kUnknown};
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
