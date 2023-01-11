// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SYSTEM_STATISTICS_PROVIDER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_SYSTEM_STATISTICS_PROVIDER_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/system/name_value_pairs_parser.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::system {

// Result of loading values from the cached VPD file.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SYSTEM)
extern const char kMetricVpdCacheReadResult[];

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

    // Binary to fake crossystem tool with arguments. E.g. echo.
    base::CommandLine crossystem_tool{base::CommandLine::NO_PROGRAM};

    base::FilePath machine_info_filepath;
    base::FilePath vpd_echo_filepath;
    base::FilePath vpd_filepath;
    base::FilePath vpd_status_filepath;
    base::FilePath oem_manifest_filepath;
    base::FilePath cros_regions_filepath;
  };

  // This enum is used to define the buckets for an enumerated UMA histogram.
  // Hence,
  //   (a) existing enumerated constants should never be deleted or reordered,
  //   and
  //   (b) new constants should only be appended at the end of the enumeration
  //       (update tools/metrics/histograms/enums.xml as well).
  enum class VpdCacheReadResult {
    kSuccess = 0,
    KMissing = 1,
    kParseFailed = 2,
    kMaxValue = kParseFailed,
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
  absl::optional<base::StringPiece> GetMachineStatistic(
      base::StringPiece name) override;
  FlagValue GetMachineFlag(base::StringPiece name) override;

  void Shutdown() override;

  // Returns true when Chrome OS is running in a VM. NOTE: if crossystem is not
  // installed it will return false even if Chrome OS is running in a VM.
  bool IsRunningOnVm() override;

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
  bool WaitForStatisticsLoaded();

  // Loads the machine statistics off of disk. Runs on the file thread.
  void LoadMachineStatistics(bool load_oem_manifest);

  // Loads calls the crossystem tool and loads statistics from its output.
  void LoadCrossystemTool();

  // Loads the machine info statistics off of disk. Runs on the file thread.
  void LoadMachineInfoFile();

  // Loads the VPD statistics off of disk. Runs on the file thread.
  void LoadVpdFiles();

  // Loads the OEM statistics off of disk. Runs on the file thread.
  void LoadOemManifestFromFile(const base::FilePath& file);

  // Loads regional data off of disk. Runs on the file thread.
  void LoadRegionsFile(const base::FilePath& filename,
                       base::StringPiece region);

  // Extracts known data from `regional_data_`.
  absl::optional<base::StringPiece> GetRegionalInformation(
      base::StringPiece name) const;

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
  // 1. Status file or VPD file is missing: both RO_VPD and RW_VPD are
  //    considered being invalid.
  // 2. Partition key is missing in the status file: corresponding partition is
  //    considered being invalid.
  // 3. Partition key has invalid value: corresponding partition is considered
  //    being invalid.
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
