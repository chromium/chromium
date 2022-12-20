// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/statistics_provider_impl.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::system {

namespace {

// Echo is used to fake crossystem tool.
constexpr char kEchoCmd[] = "/bin/echo";
constexpr char kLsbReleaseContent[] = "CHROMEOS_RELEASE_NAME=Chromium OS\n";
constexpr char kInvalidLsbReleaseContent[] = "Just empty";

constexpr char kCrossystemToolFormat[] = "%s = %s   # %s\n";
constexpr char kMachineInfoFormat[] = "\"%s\"=\"%s\"\n";
constexpr char kVpdFormat[] = "\"%s\"=\"%s\"\n";

constexpr char kVpdRoPartitionStatusKey[] = "RO_VPD_status";
constexpr char kVpdRwPartitionStatusKey[] = "RW_VPD_status";

// Creates a file named with `filename` in the temp dir and fills it with
// `content`. Returns path to the created file.
base::FilePath CreateFileInTempDir(const std::string& content,
                                   const base::ScopedTempDir& temp_dir) {
  DCHECK(temp_dir.IsValid());
  base::FilePath filepath;
  base::File file =
      base::CreateAndOpenTemporaryFileInDir(temp_dir.GetPath(), &filepath);
  DCHECK(file.IsValid());
  DCHECK(!filepath.empty());

  int length = base::WriteFile(filepath, content);
  DCHECK_GE(length, 0);

  return filepath;
}

void LoadStatistics(StatisticsProviderImpl* provider, bool load_oem_manifest) {
  base::RunLoop loading_loop;
  provider->ScheduleOnMachineStatisticsLoaded(loading_loop.QuitClosure());
  provider->StartLoadingMachineStatistics(load_oem_manifest);
  loading_loop.Run();
}

class SourcesBuilder {
 public:
  explicit SourcesBuilder(const base::ScopedTempDir& temp_dir)
      : temp_dir_(temp_dir) {}

  SourcesBuilder(const SourcesBuilder&) = delete;
  SourcesBuilder& operator=(const SourcesBuilder&) = delete;

  SourcesBuilder& set_crossystem_tool(const base::CommandLine& tool_cmd) {
    sources_.crossystem_tool = tool_cmd;
    return *this;
  }

  SourcesBuilder& set_machine_info(const base::FilePath& filepath) {
    sources_.machine_info_filepath = filepath;
    return *this;
  }

  SourcesBuilder& set_vpd_echo(const base::FilePath& filepath) {
    sources_.vpd_echo_filepath = filepath;
    return *this;
  }

  SourcesBuilder& set_vpd(const base::FilePath& filepath) {
    sources_.vpd_filepath = filepath;
    return *this;
  }

  SourcesBuilder& set_vpd_status(const base::FilePath& filepath) {
    sources_.vpd_status_filepath = filepath;
    return *this;
  }

  SourcesBuilder& set_oem_manifest(const base::FilePath& filepath) {
    sources_.oem_manifest_filepath = filepath;
    return *this;
  }

  SourcesBuilder& set_cros_regions(const base::FilePath& filepath) {
    sources_.cros_regions_filepath = filepath;
    return *this;
  }

  StatisticsProviderImpl::StatisticsSources Build() {
    if (sources_.crossystem_tool.GetProgram().empty()) {
      sources_.crossystem_tool = base::CommandLine(base::FilePath(kEchoCmd));
    }

    if (sources_.machine_info_filepath.empty()) {
      sources_.machine_info_filepath = CreateFileInTempDir("", temp_dir_);
    }

    if (sources_.vpd_echo_filepath.empty()) {
      sources_.vpd_echo_filepath = CreateFileInTempDir("", temp_dir_);
    }

    if (sources_.vpd_filepath.empty()) {
      sources_.vpd_filepath = CreateFileInTempDir("", temp_dir_);
    }

    if (sources_.vpd_status_filepath.empty()) {
      sources_.vpd_status_filepath = CreateFileInTempDir("", temp_dir_);
    }

    if (sources_.oem_manifest_filepath.empty()) {
      sources_.oem_manifest_filepath = CreateFileInTempDir("", temp_dir_);
    }

    if (sources_.cros_regions_filepath.empty()) {
      sources_.cros_regions_filepath = CreateFileInTempDir("", temp_dir_);
    }

    return std::move(sources_);
  }

 private:
  const base::ScopedTempDir& temp_dir_;
  StatisticsProviderImpl::StatisticsSources sources_;
};

}  // namespace

class StatisticsProviderImplTest : public testing::Test {
 protected:
  StatisticsProviderImplTest() { SetupFiles(); }

  const base::ScopedTempDir& temp_dir() const { return temp_dir_; }

 private:
  void SetupFiles() { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;

  base::test::TaskEnvironment test_task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Test that the provider loads statistics from the tool if they have correct
// format, and ignores statistics errors. The crossystem tool is faked with
// echo command printing formatted statistics.
TEST_F(StatisticsProviderImplTest, LoadsStatisticsFromCrossystemTool) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kEchoArgs =
      base::StringPrintf(kCrossystemToolFormat, "crossystem_key_1",
                         "crossystem_value_1", "Valid statistic") +
      base::StringPrintf(kCrossystemToolFormat, "crossystem_key_2",
                         "crossystem_value_2", "Valid statistic") +
      base::StringPrintf(kCrossystemToolFormat, "crossystem_key_3", "(error)",
                         "Invalid statistic to be erased") +
      "crossystem_key_4 : invalid_separator # Invalid statistic\n" +
      base::StringPrintf(kCrossystemToolFormat, "crossystem_key_5", "(error)",
                         "Invalid statistic to be erased");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_crossystem_tool(base::CommandLine({kEchoCmd, kEchoArgs}))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic("crossystem_key_1"),
            "crossystem_value_1");
  EXPECT_EQ(provider->GetMachineStatistic("crossystem_key_2"),
            "crossystem_value_2");
  EXPECT_FALSE(provider->GetMachineStatistic("crossystem_key_3"));
  EXPECT_FALSE(provider->GetMachineStatistic("crossystem_key_4"));
  EXPECT_FALSE(provider->GetMachineStatistic("crossystem_key_5"));
}

// Tests that provider has special handling for the firmware write protect key
// loaded from crossystem tool.
TEST_F(StatisticsProviderImplTest,
       LoadsFirmwareWriteProtectCurrentKeyFromOtherSourceThanCrossystemTool) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kEchoArgs =
      base::StringPrintf(kCrossystemToolFormat, kFirmwareWriteProtectCurrentKey,
                         "crossytem_value", "");

  const std::string kMachineInfoStatistics =
      base::StringPrintf(kMachineInfoFormat, kFirmwareWriteProtectCurrentKey,
                         "machine_info_value");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_crossystem_tool(base::CommandLine({kEchoCmd, kEchoArgs}))
          .set_machine_info(
              CreateFileInTempDir(kMachineInfoStatistics, temp_dir()))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic(kFirmwareWriteProtectCurrentKey),
            "machine_info_value");
}

// Tests that provider has special handling for the firmware write protect key
// loaded from crossystem tool.
TEST_F(
    StatisticsProviderImplTest,
    LoadsFirmwareWriteProtectCurrentKeyFromCrossystemToolIfNoOtherSourceHasIt) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kEchoArgs =
      base::StringPrintf(kCrossystemToolFormat, kFirmwareWriteProtectCurrentKey,
                         "crossytem_value", "");

  const std::string kMachineInfoStatistics = base::StringPrintf(
      kMachineInfoFormat, "machine_info_completely_different_key",
      "machine_info_value");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_crossystem_tool(base::CommandLine({kEchoCmd, kEchoArgs}))
          .set_machine_info(
              CreateFileInTempDir(kMachineInfoStatistics, temp_dir()))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic(kFirmwareWriteProtectCurrentKey),
            "crossytem_value");
}

// Tests that StatisticsProvider skips crossystem tool in non-ChromeOS test
// environment.
TEST_F(StatisticsProviderImplTest,
       DoesNotLoadStatisticsFromCrossystemToolIfNotRunningChromeOS) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(
      kInvalidLsbReleaseContent, base::Time());
  ASSERT_FALSE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kEchoArgs =
      base::StringPrintf(kCrossystemToolFormat, "key_1", "value_1",
                         "Valid statistic") +
      base::StringPrintf(kCrossystemToolFormat, "key_2", "value_2",
                         "Valid statistic");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_crossystem_tool(base::CommandLine({kEchoCmd, kEchoArgs}))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_FALSE(provider->GetMachineStatistic("crossystem_key_1"));
  EXPECT_FALSE(provider->GetMachineStatistic("crossystem_key_2"));
}

// Test that the provider loads statistics from machine info file if they have
// correct format.
TEST_F(StatisticsProviderImplTest, LoadsStatisticsFromMachineInfoFile) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kMachineInfoStatistics =
      base::StringPrintf(kMachineInfoFormat, "machine_info_key_1",
                         "machine_info_value_1") +
      base::StringPrintf(kMachineInfoFormat, "machine_info_key_2",
                         "machine_info_value_2") +
      "machine_info_malformed_key_3 = machine_info_malformed_value_3\n" +
      "machine_info_malformed_key_4 : \"machine_info_malformed_value_4\"\n" +
      base::StringPrintf(kMachineInfoFormat, "machine_info_key_5",
                         "machine_info_value_5");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_machine_info(
              CreateFileInTempDir(kMachineInfoStatistics, temp_dir()))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic("machine_info_key_1"),
            "machine_info_value_1");
  EXPECT_EQ(provider->GetMachineStatistic("machine_info_key_2"),
            "machine_info_value_2");
  EXPECT_FALSE(provider->GetMachineStatistic("machine_info_malformed_key_3"));
  EXPECT_FALSE(provider->GetMachineStatistic("machine_info_malformed_key_4"));
  EXPECT_EQ(provider->GetMachineStatistic("machine_info_key_5"),
            "machine_info_value_5");
}

// Tests that StatisticsProvider generates stub statistics file for machine info
// in in non-ChromeOS test environment.
TEST_F(StatisticsProviderImplTest,
       GeneratesStubMachineInfoFileIfNotRunningChromeOS) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(
      kInvalidLsbReleaseContent, base::Time());
  ASSERT_FALSE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const base::FilePath kMachineInfoFilepath =
      temp_dir().GetPath().Append("machine_info");
  ASSERT_FALSE(base::PathExists(kMachineInfoFilepath));

  const StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir()).set_machine_info(kMachineInfoFilepath).Build();

  // Load statistics.
  auto provider =
      StatisticsProviderImpl::CreateProviderForTesting(testing_sources);
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  const auto initial_machine_id = provider->GetMachineID();
  EXPECT_TRUE(initial_machine_id && !initial_machine_id->empty());

  // Check stub file exists.
  EXPECT_TRUE(base::PathExists(kMachineInfoFilepath));

  // Current provider is going to be destroyed, copy it's machine id.
  const std::string initial_machine_id_string =
      std::string(initial_machine_id.value_or(""));

  // Check fresh provider.
  provider = StatisticsProviderImpl::CreateProviderForTesting(testing_sources);
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Expect the same statistic as initial.
  EXPECT_EQ(provider->GetMachineID(), initial_machine_id_string);
}

// Test that the provider loads statistics from VPD echo and VPD file if they
// have correct format. Test that the provider records correct metrics.
TEST_F(StatisticsProviderImplTest, LoadsStatisticsFromVpdFile) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kVpdEchoStatistics =
      base::StringPrintf(kVpdFormat, "vpd_echo_key_1", "vpd_echo_value_1") +
      base::StringPrintf(kVpdFormat, "vpd_echo_key_2", "vpd_echo_value_2") +
      "vpd_echo_malformed_key_3 = vpd_echo_malformed_value_3\n" +
      "vpd_echo_malformed_key_4 : \"vpd_echo_malformed_value_4\"\n" +
      base::StringPrintf(kVpdFormat, "vpd_echo_key_5", "vpd_echo_value_5");

  // Malformed values are skipped here so that provider records success metric
  // for parsing the VPD file. Malformed values are tested in a separate test
  // case.
  const std::string kVpdStatistics =
      base::StringPrintf(kVpdFormat, "vpd_key_1", "vpd_value_1") +
      base::StringPrintf(kVpdFormat, "vpd_key_2", "vpd_value_2") +
      base::StringPrintf(kVpdFormat, "vpd_key_3", "vpd_value_3");

  const std::string kVpdStatusStatistics =
      base::StringPrintf(kVpdFormat, kVpdRoPartitionStatusKey, "0") +
      base::StringPrintf(kVpdFormat, kVpdRwPartitionStatusKey, "0");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_vpd_echo(CreateFileInTempDir(kVpdEchoStatistics, temp_dir()))
          .set_vpd(CreateFileInTempDir(kVpdStatistics, temp_dir()))
          .set_vpd_status(CreateFileInTempDir(kVpdStatusStatistics, temp_dir()))
          .Build();

  // Load statistics.
  base::HistogramTester histogram_tester;
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic("vpd_echo_key_1"),
            "vpd_echo_value_1");
  EXPECT_EQ(provider->GetMachineStatistic("vpd_echo_key_2"),
            "vpd_echo_value_2");
  EXPECT_FALSE(provider->GetMachineStatistic("vpd_echo_malformed_key_3"));
  EXPECT_FALSE(provider->GetMachineStatistic("vpd_echo_malformed_key_4"));
  EXPECT_EQ(provider->GetMachineStatistic("vpd_echo_key_5"),
            "vpd_echo_value_5");

  EXPECT_EQ(provider->GetMachineStatistic("vpd_key_1"), "vpd_value_1");
  EXPECT_EQ(provider->GetMachineStatistic("vpd_key_2"), "vpd_value_2");
  EXPECT_EQ(provider->GetMachineStatistic("vpd_key_3"), "vpd_value_3");

  // Check histogram recordings.
  histogram_tester.ExpectUniqueSample(
      kMetricVpdCacheReadResult,
      StatisticsProviderImpl::VpdCacheReadResult::kSuccess,
      /*expected_bucket_count=*/1);

  // Check VPD status.
  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kValid);
}

// Test that the provider records correct metrics when VPD file is missing.
TEST_F(StatisticsProviderImplTest, RecordsErrorIfVpdFileIsMissing) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const base::FilePath kNonExistingVpdFilepath =
      temp_dir().GetPath().Append("vpd_does_not_exist");
  ASSERT_FALSE(base::PathExists(kNonExistingVpdFilepath));

  // Setup valid VPD status file to ensure the invalid VPD status comes from the
  // missing VPD file.
  const std::string kVpdStatusStatistics =
      base::StringPrintf(kVpdFormat, kVpdRoPartitionStatusKey, "0") +
      base::StringPrintf(kVpdFormat, kVpdRwPartitionStatusKey, "0");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_vpd(std::move(kNonExistingVpdFilepath))
          .set_vpd_status(CreateFileInTempDir(kVpdStatusStatistics, temp_dir()))
          .Build();

  // Load statistics.
  base::HistogramTester histogram_tester;
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check histogram recordings.
  histogram_tester.ExpectBucketCount(
      kMetricVpdCacheReadResult,
      StatisticsProviderImpl::VpdCacheReadResult::KMissing,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(kMetricVpdCacheReadResult,
                                    /*count=*/1);

  // Expect invalid VPD status because the VPD file is missing.
  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kInvalid);
}

// Test that the provider records correct metrics when VPD file has incorrect
// values.
TEST_F(StatisticsProviderImplTest, RecordsErrorIfVpdFileIsMalformed) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kVpdStatistics =
      base::StringPrintf(kVpdFormat, "vpd_key_1", "vpd_value_1") +
      "vpd_malformed_key_2 = vpd_malformed_value_2\n" +
      "vpd_malformed_key_3 : \"vpd_malformed_value_3\"\n";

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_vpd(CreateFileInTempDir(kVpdStatistics, temp_dir()))
          .Build();

  // Load statistics.
  base::HistogramTester histogram_tester;
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic("vpd_key_1"), "vpd_value_1");
  EXPECT_FALSE(provider->GetMachineStatistic("vpd_malformed_key_2"));
  EXPECT_FALSE(provider->GetMachineStatistic("vpd_malformed_key_3"));

  // Check histogram recordings.
  histogram_tester.ExpectUniqueSample(
      kMetricVpdCacheReadResult,
      StatisticsProviderImpl::VpdCacheReadResult::kParseFailed,
      /*expected_bucket_count=*/1);

  // Expect invalid VPD status because VPD status file does not contain status
  // keys.
  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kInvalid);
}

// Tests that StatisticsProvider generates stub statistics file for VPD
// in in non-ChromeOS test environment.
TEST_F(StatisticsProviderImplTest, GeneratesStubVpdFileIfNotRunningChromeOS) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(
      kInvalidLsbReleaseContent, base::Time());
  ASSERT_FALSE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const base::FilePath kVpdFilepath = temp_dir().GetPath().Append("vpd");
  ASSERT_FALSE(base::PathExists(kVpdFilepath));

  const StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir()).set_vpd(kVpdFilepath).Build();

  // Load statistics.
  base::HistogramTester histogram_tester;
  auto provider =
      StatisticsProviderImpl::CreateProviderForTesting(testing_sources);
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  const auto initial_activate_date =
      provider->GetMachineStatistic(kActivateDateKey);
  EXPECT_TRUE(initial_activate_date);

  // Check stub file exists.
  EXPECT_TRUE(base::PathExists(kVpdFilepath));

  // The provider shall not record in non-chromeos environment.
  histogram_tester.ExpectTotalCount(kMetricVpdCacheReadResult,
                                    /*count=*/0);

  // Expect invalid VPD status because VPD status file does not contain status
  // keys.
  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kInvalid);

  // Current provider is going to be destroyed, copy it's activate date.
  const std::string initial_activate_date_string =
      std::string(initial_activate_date.value_or(""));

  // Check fresh provider.
  provider = StatisticsProviderImpl::CreateProviderForTesting(testing_sources);
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Expect the same statistic as initial.
  EXPECT_EQ(provider->GetMachineStatistic(kActivateDateKey),
            initial_activate_date_string);

  // The provider shall not record in non-chromeos environment.
  histogram_tester.ExpectTotalCount(kMetricVpdCacheReadResult,
                                    /*count=*/0);

  // Expect invalid VPD status because VPD status file does not contain status
  // keys.
  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kInvalid);
}

// Test that the provider returns correct VPD status when VPD status file does
// not exist.
TEST_F(StatisticsProviderImplTest,
       ReturnsInvalidVpdStatusWithNonExistingStatusFile) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const base::FilePath kNonExistingVpdStatusFilepath =
      temp_dir().GetPath().Append("vpd_does_not_exist");
  ASSERT_FALSE(base::PathExists(kNonExistingVpdStatusFilepath));

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_vpd_status(kNonExistingVpdStatusFilepath)
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));

  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kUnknown);

  base::RunLoop loading_loop;
  provider->ScheduleOnMachineStatisticsLoaded(loading_loop.QuitClosure());
  provider->StartLoadingMachineStatistics(/*load_oem_manifest=*/false);
  loading_loop.Run();

  // Check VPD status.
  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kInvalid);
}

// Test that the provider returns correct VPD status with invalid RO VPD status
// is status file.
TEST_F(StatisticsProviderImplTest, ReturnsInvalidRoVpdStatus) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kInvalidRoVpdStatus =
      base::StringPrintf(kVpdFormat, kVpdRoPartitionStatusKey, "1") +
      base::StringPrintf(kVpdFormat, kVpdRwPartitionStatusKey, "0");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_vpd_status(CreateFileInTempDir(kInvalidRoVpdStatus, temp_dir()))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));

  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kUnknown);

  base::RunLoop loading_loop;
  provider->ScheduleOnMachineStatisticsLoaded(loading_loop.QuitClosure());
  provider->StartLoadingMachineStatistics(/*load_oem_manifest=*/false);
  loading_loop.Run();

  // Check VPD status.
  EXPECT_EQ(provider->GetVpdStatus(),
            StatisticsProvider::VpdStatus::kRoInvalid);
}

// Test that the provider returns correct VPD status with missing RO VPD status
// is status file.
TEST_F(StatisticsProviderImplTest, ReturnsInvalidRoVpdStatusWhenKeyIsMissing) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kInvalidRoVpdStatus =
      base::StringPrintf(kVpdFormat, kVpdRwPartitionStatusKey, "0");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_vpd_status(CreateFileInTempDir(kInvalidRoVpdStatus, temp_dir()))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));

  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kUnknown);

  base::RunLoop loading_loop;
  provider->ScheduleOnMachineStatisticsLoaded(loading_loop.QuitClosure());
  provider->StartLoadingMachineStatistics(/*load_oem_manifest=*/false);
  loading_loop.Run();

  // Check VPD status.
  EXPECT_EQ(provider->GetVpdStatus(),
            StatisticsProvider::VpdStatus::kRoInvalid);
}

// Test that the provider returns correct VPD status with invalid RW VPD status
// is status file.
TEST_F(StatisticsProviderImplTest, ReturnsInvalidRwVpdStatus) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kInvalidRwVpdStatus =
      base::StringPrintf(kVpdFormat, kVpdRoPartitionStatusKey, "0") +
      base::StringPrintf(kVpdFormat, kVpdRwPartitionStatusKey, "2");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_vpd_status(CreateFileInTempDir(kInvalidRwVpdStatus, temp_dir()))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));

  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kUnknown);

  base::RunLoop loading_loop;
  provider->ScheduleOnMachineStatisticsLoaded(loading_loop.QuitClosure());
  provider->StartLoadingMachineStatistics(/*load_oem_manifest=*/false);
  loading_loop.Run();

  // Check VPD status.
  EXPECT_EQ(provider->GetVpdStatus(),
            StatisticsProvider::VpdStatus::kRwInvalid);
}

// Test that the provider returns correct VPD status with missing RW VPD status
// is status file.
TEST_F(StatisticsProviderImplTest, ReturnsInvalidRwVpdStatusWhenKeyIsMissing) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kInvalidRwVpdStatus =
      base::StringPrintf(kVpdFormat, kVpdRoPartitionStatusKey, "0");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_vpd_status(CreateFileInTempDir(kInvalidRwVpdStatus, temp_dir()))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));

  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kUnknown);

  base::RunLoop loading_loop;
  provider->ScheduleOnMachineStatisticsLoaded(loading_loop.QuitClosure());
  provider->StartLoadingMachineStatistics(/*load_oem_manifest=*/false);
  loading_loop.Run();

  // Check VPD status.
  EXPECT_EQ(provider->GetVpdStatus(),
            StatisticsProvider::VpdStatus::kRwInvalid);
}

// Test that the provider returns correct VPD status with invalid RO and RW VPD
// statuses is status file.
TEST_F(StatisticsProviderImplTest, ReturnsInvalidVpdStatus) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kInvalidVpdStatus =
      base::StringPrintf(kVpdFormat, kVpdRoPartitionStatusKey, "3") +
      base::StringPrintf(kVpdFormat, kVpdRwPartitionStatusKey, "4");

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_vpd_status(CreateFileInTempDir(kInvalidVpdStatus, temp_dir()))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));

  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kUnknown);

  base::RunLoop loading_loop;
  provider->ScheduleOnMachineStatisticsLoaded(loading_loop.QuitClosure());
  provider->StartLoadingMachineStatistics(/*load_oem_manifest=*/false);
  loading_loop.Run();

  // Check VPD status.
  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kInvalid);
}

// Test that the provider loads correct statistics OEM file if they
// have correct format.
TEST_F(StatisticsProviderImplTest, LoadsOemManifest) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  constexpr char kOemManifestStatistics[] = R"({
    "device_requisition": "device_requisition_value",
    "not_oem_statistic_key": "not_oem_statistic_value",
    "enterprise_managed": true,
    "can_exit_enrollment": true,
    "keyboard_driven_oobe": true,
    "not_oem_flag_key": true
  })";

  const StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_oem_manifest(
              CreateFileInTempDir(kOemManifestStatistics, temp_dir()))
          .Build();

  // Load statistics without OEM flag.
  auto provider =
      StatisticsProviderImpl::CreateProviderForTesting(testing_sources);
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_FALSE(provider->GetMachineStatistic(kOemDeviceRequisitionKey));
  EXPECT_FALSE(provider->GetMachineStatistic("not_oem_statistic_key"));

  for (const auto* oem_flag :
       {kOemIsEnterpriseManagedKey, kOemCanExitEnterpriseEnrollmentKey,
        kOemKeyboardDrivenOobeKey}) {
    EXPECT_EQ(provider->GetMachineFlag(oem_flag),
              StatisticsProviderImpl::FlagValue::kUnset);
  }

  EXPECT_EQ(provider->GetMachineFlag("not_oem_flag_key"),
            StatisticsProviderImpl::FlagValue::kUnset);

  // Load statistics with OEM flag.
  provider = StatisticsProviderImpl::CreateProviderForTesting(testing_sources);
  LoadStatistics(provider.get(), /*load_oem_manifest=*/true);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic(kOemDeviceRequisitionKey),
            "device_requisition_value");
  EXPECT_FALSE(provider->GetMachineStatistic("not_oem_statistic_key"));

  for (const auto* oem_flag :
       {kOemIsEnterpriseManagedKey, kOemCanExitEnterpriseEnrollmentKey,
        kOemKeyboardDrivenOobeKey}) {
    EXPECT_EQ(provider->GetMachineFlag(oem_flag),
              StatisticsProviderImpl::FlagValue::kTrue);
  }

  EXPECT_EQ(provider->GetMachineFlag("not_oem_flag_key"),
            StatisticsProviderImpl::FlagValue::kUnset);
}

// Test that the provider loads prefers OEM manifest file set by command line.
TEST_F(StatisticsProviderImplTest, LoadsOemManifestFromCommandLine) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  constexpr char kOemManifestStatistics[] = R"({
    "device_requisition": "device_requisition_value",
    "not_oem_statistic_key": "not_oem_statistic_value",
    "enterprise_managed": true,
    "can_exit_enrollment": true,
    "keyboard_driven_oobe": true,
    "not_oem_flag_key": true
  })";

  constexpr char kOemManifestCommandLineStatistics[] = R"({
    "device_requisition": "device_requisition_command_line_value",
    "not_oem_statistic_key": "not_oem_statistic_value",
    "enterprise_managed": false,
    "can_exit_enrollment": false,
    "keyboard_driven_oobe": false,
    "not_oem_flag_key": true
  })";

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_oem_manifest(
              CreateFileInTempDir(kOemManifestStatistics, temp_dir()))
          .Build();

  const base::FilePath oem_manifest_filepath_for_commandline =
      CreateFileInTempDir(kOemManifestCommandLineStatistics, temp_dir());

  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      switches::kAppOemManifestFile, oem_manifest_filepath_for_commandline);

  // Load statistics with OEM flag.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/true);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic(kOemDeviceRequisitionKey),
            "device_requisition_command_line_value");
  EXPECT_FALSE(provider->GetMachineStatistic("not_oem_statistic_key"));

  for (const auto* oem_flag :
       {kOemIsEnterpriseManagedKey, kOemCanExitEnterpriseEnrollmentKey,
        kOemKeyboardDrivenOobeKey}) {
    EXPECT_EQ(provider->GetMachineFlag(oem_flag),
              StatisticsProvider::FlagValue::kFalse);
  }

  EXPECT_EQ(provider->GetMachineFlag("not_oem_flag_key"),
            StatisticsProviderImpl::FlagValue::kUnset);
}

// Tests that StatisticsProvider skips OEM manifest statistics in non-ChromeOS
// test environment.
TEST_F(StatisticsProviderImplTest, DoesNotLoadOemManifestIfNotRunningChromeOS) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(
      kInvalidLsbReleaseContent, base::Time());
  ASSERT_FALSE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  constexpr char kOemManifestStatistics[] = R"({
    "device_requisition": "device_requisition_value",
    "enterprise_managed": true,
    "can_exit_enrollment": true,
    "keyboard_driven_oobe": true,
  })";

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_oem_manifest(
              CreateFileInTempDir(kOemManifestStatistics, temp_dir()))
          .Build();

  // Load statistics with OEM flag.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/true);

  // Check statistics.
  EXPECT_FALSE(provider->GetMachineStatistic(kOemDeviceRequisitionKey));

  for (const auto* oem_flag :
       {kOemIsEnterpriseManagedKey, kOemCanExitEnterpriseEnrollmentKey,
        kOemKeyboardDrivenOobeKey}) {
    EXPECT_EQ(provider->GetMachineFlag(oem_flag),
              StatisticsProviderImpl::FlagValue::kUnset);
  }
}

// Test that the provider loads statistics from regions file if they
// have correct format.
TEST_F(StatisticsProviderImplTest, LoadsRegionsFile) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kMachineInfoStatistics =
      base::StringPrintf(kMachineInfoFormat, kRegionKey, "region_value");

  // NOTE: keys in regions JSON are different from the ones that are provided.
  // See `kInitialLocaleKey` and `kLocalesPath`, and
  // `GetInitialTimezoneFromRegionalData` for example.
  constexpr char kRegionsStatistics[] = R"({
    "region_value": {
      "locales": ["locale_1", "locale_2", "locale_3"],
      "keyboards": ["layout_1", "layout_2", "layout_3"],
      "keyboard_mechanical_layout": "mechanical_layout",
      "time_zones": ["timezone_1", "timezone_2", "timezone_3"],
      "non_region_key": "non_region_value"
    }
  })";

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_machine_info(
              CreateFileInTempDir(kMachineInfoStatistics, temp_dir()))
          .set_cros_regions(CreateFileInTempDir(kRegionsStatistics, temp_dir()))
          .Build();

  // Load statistics.
  auto provider =
      StatisticsProviderImpl::CreateProviderForTesting(testing_sources);
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic(kRegionKey), "region_value");
  EXPECT_EQ(provider->GetMachineStatistic(kInitialLocaleKey),
            "locale_1,locale_2,locale_3");
  EXPECT_EQ(provider->GetMachineStatistic(kKeyboardLayoutKey),
            "layout_1,layout_2,layout_3");
  EXPECT_EQ(provider->GetMachineStatistic(kKeyboardMechanicalLayoutKey),
            "mechanical_layout");
  EXPECT_EQ(provider->GetMachineStatistic(kInitialTimezoneKey), "timezone_1");
  EXPECT_FALSE(provider->GetMachineStatistic("non_region_key"));
}

// Test that the provider loads statistics from regions file from correct region
// set by command line if statistics have correct format.
TEST_F(StatisticsProviderImplTest, SetsRegionFromCommandLine) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kMachineInfoStatistics =
      base::StringPrintf(kMachineInfoFormat, kRegionKey, "region_value") +
      base::StringPrintf(kMachineInfoFormat, kInitialLocaleKey,
                         "machine_info_locale") +
      base::StringPrintf(kMachineInfoFormat, kKeyboardLayoutKey,
                         "machine_info_layout") +
      base::StringPrintf(kMachineInfoFormat, kKeyboardMechanicalLayoutKey,
                         "machine_info_mechanical_layout") +
      base::StringPrintf(kMachineInfoFormat, kInitialTimezoneKey,
                         "machine_info_mechanical_timezone") +
      base::StringPrintf(kMachineInfoFormat, "non_region_key",
                         "machine_info_region_value");

  constexpr char kRegionsStatistics[] = R"({
    "region_switch": {
      "locales": ["locale_1", "locale_2", "locale_3"],
      "keyboards": ["layout_1", "layout_2", "layout_3"],
      "keyboard_mechanical_layout": "mechanical_layout",
      "time_zones": ["timezone_1", "timezone_2", "timezone_3"],
      "non_region_key": "non_region_value"
    }
  })";

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_machine_info(
              CreateFileInTempDir(kMachineInfoStatistics, temp_dir()))
          .set_cros_regions(CreateFileInTempDir(kRegionsStatistics, temp_dir()))
          .Build();

  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(switches::kCrosRegion,
                                                          "region_switch");

  // Load statistics.
  auto provider =
      StatisticsProviderImpl::CreateProviderForTesting(testing_sources);
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic(kRegionKey), "region_switch");
  EXPECT_EQ(provider->GetMachineStatistic(kInitialLocaleKey),
            "locale_1,locale_2,locale_3");
  EXPECT_EQ(provider->GetMachineStatistic(kKeyboardLayoutKey),
            "layout_1,layout_2,layout_3");
  EXPECT_EQ(provider->GetMachineStatistic(kKeyboardMechanicalLayoutKey),
            "mechanical_layout");
  EXPECT_EQ(provider->GetMachineStatistic(kInitialTimezoneKey), "timezone_1");
  EXPECT_EQ(provider->GetMachineStatistic("non_region_key"),
            "machine_info_region_value");
}

// Test that the provider does not load region statistics when region file does
// not exist.
TEST_F(StatisticsProviderImplTest, DoesNotLoadRegionsFileWhenFileIsMissing) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kMachineInfoStatistics =
      base::StringPrintf(kMachineInfoFormat, kRegionKey, "region_value");

  const base::FilePath kNonExistingRegionsFilepath =
      temp_dir().GetPath().Append("vpd_does_not_exist");
  ASSERT_FALSE(base::PathExists(kNonExistingRegionsFilepath));

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_machine_info(
              CreateFileInTempDir(kMachineInfoStatistics, temp_dir()))
          .set_cros_regions(kNonExistingRegionsFilepath)
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic(kRegionKey), "region_value");
  EXPECT_FALSE(provider->GetMachineStatistic(kInitialLocaleKey));
  EXPECT_FALSE(provider->GetMachineStatistic(kKeyboardLayoutKey));
  EXPECT_FALSE(provider->GetMachineStatistic(kKeyboardMechanicalLayoutKey));
  EXPECT_FALSE(provider->GetMachineStatistic(kInitialTimezoneKey));
}

// Test that the provider does not load region statistics when region file has
// incorrect formatting.
TEST_F(StatisticsProviderImplTest, DoesNotLoadRegionsFileWhenFileIsMalformed) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kMachineInfoStatistics =
      base::StringPrintf(kMachineInfoFormat, kRegionKey, "region_value");

  constexpr char kRegionsStatistics[] = R"({
    "region_value": ["list", "is", "not", "a" "dictionary"]
  })";

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_machine_info(
              CreateFileInTempDir(kMachineInfoStatistics, temp_dir()))
          .set_cros_regions(CreateFileInTempDir(kRegionsStatistics, temp_dir()))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic(kRegionKey), "region_value");
  EXPECT_FALSE(provider->GetMachineStatistic(kInitialLocaleKey));
  EXPECT_FALSE(provider->GetMachineStatistic(kKeyboardLayoutKey));
  EXPECT_FALSE(provider->GetMachineStatistic(kKeyboardMechanicalLayoutKey));
  EXPECT_FALSE(provider->GetMachineStatistic(kInitialTimezoneKey));
}

// Test that the provider does not load region statistics when region file does
// not have correct regions key.
TEST_F(StatisticsProviderImplTest,
       DoesNotLoadRegionsFileWhenRegionKeyIsMissing) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const std::string kMachineInfoStatistics =
      base::StringPrintf(kMachineInfoFormat, kRegionKey, "region_value");

  constexpr char kRegionsStatistics[] = R"({
    "different_region_value": {
      "locales": ["locale_1", "locale_2", "locale_3"],
      "keyboards": ["layout_1", "layout_2", "layout_3"],
      "keyboard_mechanical_layout": "mechanical_layout",
      "time_zones": ["timezone_1", "timezone_2", "timezone_3"],
      "non_region_key": "non_region_value"
    }
  })";

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .set_machine_info(
              CreateFileInTempDir(kMachineInfoStatistics, temp_dir()))
          .set_cros_regions(CreateFileInTempDir(kRegionsStatistics, temp_dir()))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic(kRegionKey), "region_value");
  EXPECT_FALSE(provider->GetMachineStatistic(kInitialLocaleKey));
  EXPECT_FALSE(provider->GetMachineStatistic(kKeyboardLayoutKey));
  EXPECT_FALSE(provider->GetMachineStatistic(kKeyboardMechanicalLayoutKey));
  EXPECT_FALSE(provider->GetMachineStatistic(kInitialTimezoneKey));
}

}  // namespace ash::system
