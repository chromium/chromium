// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYSTEM_STATISTICS_PROVIDER_IMPL_H_
#define CHROMEOS_SYSTEM_STATISTICS_PROVIDER_IMPL_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chromeos/system/name_value_pairs_parser.h"
#include "chromeos/system/statistics_provider.h"

namespace chromeos::system {

// Result of loading values from the cached VPD file.
COMPONENT_EXPORT(CHROMEOS_SYSTEM) extern const char kMetricVpdCacheReadResult[];

class COMPONENT_EXPORT(CHROMEOS_SYSTEM) StatisticsProviderImpl
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
  bool GetMachineStatistic(const std::string& name,
                           std::string* result) override;
  bool GetMachineFlag(const std::string& name, bool* result) override;
  void Shutdown() override;

  // Returns true when Chrome OS is running in a VM. NOTE: if crossystem is not
  // installed it will return false even if Chrome OS is running in a VM.
  bool IsRunningOnVm() override;

 private:
  using MachineFlags = std::map<std::string, bool>;
  using RegionDataExtractor = bool (*)(const base::Value&, std::string*);

  explicit StatisticsProviderImpl(StatisticsSources sources);

  // Called when statistics have finished loading. Unblocks pending calls to
  // WaitForStatisticsLoaded() and schedules callbacks passed to
  // ScheduleOnMachineStatisticsLoaded().
  void SignalStatisticsLoaded();

  // Waits up to `kTimeoutSecs` for statistics to be loaded. Returns true if
  // they were loaded successfully.
  bool WaitForStatisticsLoaded();

  // Loads the machine statistics off of disk. Runs on the file thread.
  void LoadMachineStatistics(bool load_oem_manifest);

  // Loads the OEM statistics off of disk. Runs on the file thread.
  void LoadOemManifestFromFile(const base::FilePath& file);

  // Loads regional data off of disk. Runs on the file thread.
  void LoadRegionsFile(const base::FilePath& filename);

  // Extracts known data from regional_data_;
  // Returns true on success;
  bool GetRegionalInformation(const std::string& name,
                              std::string* result) const;

  // Returns extractor from regional_data_extractors_ or nullptr.
  RegionDataExtractor GetRegionalDataExtractor(const std::string& name) const;

  StatisticsSources sources_;

  bool load_statistics_started_;
  NameValuePairsParser::NameValueMap machine_info_;
  MachineFlags machine_flags_;
  base::AtomicFlag cancellation_flag_;
  bool oem_manifest_loaded_;
  std::string region_;
  base::Value region_dict_;
  base::flat_map<std::string, RegionDataExtractor> regional_data_extractors_;

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

}  // namespace chromeos::system

#endif  // CHROMEOS_SYSTEM_STATISTICS_PROVIDER_IMPL_H_
