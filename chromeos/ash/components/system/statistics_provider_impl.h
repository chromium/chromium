// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_STATISTICS_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_STATISTICS_PROVIDER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/system/name_value_pairs_parser.h"
#include "chromeos/ash/components/system/statistics_provider.h"

namespace ash::system {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM) StatisticsProviderImpl
    : public StatisticsProvider {
 public:
  struct StatisticsSources {
    StatisticsSources();
    ~StatisticsSources();

    StatisticsSources(const StatisticsSources& other);
    StatisticsSources& operator=(const StatisticsSources& other);

    StatisticsSources(StatisticsSources&& other);
    StatisticsSources& operator=(StatisticsSources&& other);

    // Command line for retrieving a filtered list of VPD key/value pairs. (Or,
    // a fake tool.)
    base::CommandLine vpd_tool{base::CommandLine::NO_PROGRAM};
    // Binary to fake crossystem tool with arguments. E.g. echo.
    base::CommandLine crossystem_tool{base::CommandLine::NO_PROGRAM};

    base::FilePath machine_info_filepath;
    base::FilePath oem_manifest_filepath;
    base::FilePath cros_regions_filepath;
  };

  // Constructs a provider with given `testing_sources` for testing purposes.
  static std::unique_ptr<StatisticsProviderImpl> CreateProviderForTesting(
      StatisticsSources testing_sources);

  // Constructs a provider with default source paths.
  StatisticsProviderImpl();
  ~StatisticsProviderImpl() override;

  StatisticsProviderImpl(const StatisticsProviderImpl&) = delete;
  StatisticsProviderImpl& operator=(const StatisticsProviderImpl&) = delete;

  // StatisticsProvider implementation:
  void StartLoadingMachineStatistics(bool load_oem_manifest) override;
  void ScheduleOnMachineStatisticsLoaded(base::OnceClosure callback) override;

  // If `ash::switches::kCrosRegion` switch is set, looks for the requested
  // statistic in the region file and ignores any other sources. Otherwise
  // returns the statistic from the first matching source.
  std::optional<std::string_view> GetMachineStatistic(
      std::string_view name) override;
  FlagValue GetMachineFlag(std::string_view name) override;

  void Shutdown() override;

  // Returns true when Chrome OS is running in a VM. NOTE: if crossystem is not
  // installed it will return false even if Chrome OS is running in a VM.
  bool IsRunningOnVm() override;

  // Returns true when ChromeOS is running in debug mode. NOTE: if crossystem
  // is not installed it will return false even if ChromeOS is running in debug
  // mode.
  bool IsCrosDebugMode() override;

  VpdStatus GetVpdStatus() const override;

 private:
  using MachineFlags = base::flat_map<std::string, bool>;

  explicit StatisticsProviderImpl(StatisticsSources sources);

  // Called when statistics have finished loading. Unblocks pending calls to
  // `WaitForStatisticsLoaded()` and schedules callbacks passed to
  // `ScheduleOnMachineStatisticsLoaded()`.
  void SignalStatisticsLoaded();

  // Waits up to `kTimeoutSecs` for statistics to be loaded. Returns true if
  // they were loaded successfully.
  bool WaitForStatisticsLoaded(std::string_view statistic_name);

  // Loads the machine statistics off of disk. Runs on the file thread.
  void LoadMachineStatistics(bool load_oem_manifest);

  // Loads calls the crossystem tool and loads statistics from its output.
  void LoadCrossystemTool();

  // Loads the machine info statistics off of disk. Runs on the file thread.
  void LoadMachineInfoFile();

  // Loads the VPD statistics. Runs on the file thread.
  void LoadVpd();

  // Loads the OEM statistics off of disk. Runs on the file thread.
  void LoadOemManifestFromFile(const base::FilePath& file);

  // Loads regional data off of disk. Runs on the file thread.
  void LoadRegionsFile(const base::FilePath& filename, std::string_view region);

  // Extracts known data from `regional_data_`.
  std::optional<std::string_view> GetRegionalInformation(
      std::string_view name) const;

  StatisticsSources sources_;

  bool load_statistics_started_;
  NameValuePairsParser::NameValueMap machine_info_;
  MachineFlags machine_flags_;
  // Statistics extracted from region file and associated with `kRegionKey`
  // region.
  base::flat_map<std::string, std::string> region_info_;
  base::AtomicFlag cancellation_flag_;
  bool oem_manifest_loaded_;

  // Stores VPD partitions status.
  // VPD partition or partitions are considered in invalid state if:
  // 1. The VPD dump program encounters an error.
  // 2. The region in question (RO or RW) is reported invalid (e.g., erased or
  //    corrupted).
  VpdStatus vpd_status_{VpdStatus::kUnknown};

  // Lock held when `statistics_loaded_` is signaled and when
  // `statistics_loaded_callbacks_` is accessed.
  base::Lock statistics_loaded_lock_;

  // Signaled once machine statistics are loaded. It is guaranteed that
  // `machine_info_` and `machine_flags_` don't change once this is signaled.
  base::WaitableEvent statistics_loaded_;

  // Callbacks to schedule once machine statistics are loaded.
  std::vector<
      std::pair<base::OnceClosure, scoped_refptr<base::SequencedTaskRunner>>>
      statistics_loaded_callbacks_;
};

}  // namespace ash::system

#endif  // CHROMEOS_ASH_COMPONENTS_SYSTEM_STATISTICS_PROVIDER_IMPL_H_
