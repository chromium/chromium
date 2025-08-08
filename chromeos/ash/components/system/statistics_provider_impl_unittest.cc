// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/statistics_provider_impl.h"

#include <map>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::system {

namespace {

// Echo is used to fake crossystem tool.
constexpr char kEchoCmd[] = "/bin/echo";
// `false` is used to fake the vpd tool.
constexpr char kFalseCmd[] = "/bin/false";
constexpr char kLsbReleaseContent[] = "CHROMEOS_RELEASE_NAME=Chromium OS\n";
constexpr char kInvalidLsbReleaseContent[] = "Just empty";

constexpr char kCrossystemToolFormat[] = "%s = %s   # %s\n";
constexpr char kMachineInfoFormat[] = "\"%s\"=\"%s\"\n";

// Creates a file named with `filename` in the temp dir and fills it with
// `content`. Returns path to the created file.
base::FilePath CreateFileInTempDir(const std::string& content,
                                   const base::ScopedTempDir& temp_dir) {
  EXPECT_TRUE(temp_dir.IsValid());
  base::FilePath filepath;
  base::File file =
      base::CreateAndOpenTemporaryFileInDir(temp_dir.GetPath(), &filepath);
  EXPECT_TRUE(file.IsValid());
  EXPECT_FALSE(filepath.empty());

  EXPECT_TRUE(base::WriteFile(filepath, content));

  return filepath;
}

void LoadStatistics(StatisticsProviderImpl* provider, bool load_oem_manifest) {
  base::RunLoop loading_loop;
  provider->ScheduleOnMachineStatisticsLoaded(loading_loop.QuitClosure());
  provider->StartLoadingMachineStatistics(load_oem_manifest);
  loading_loop.Run();
}

// Exit codes for the dump_filtered_vpd utility.
enum DumpVpdExitCode {
  kValid = 0,
  kRoInvalid = 1,
  kRwInvalid = 2,
  kBothInvalid = kRoInvalid | kRwInvalid,
};

base::CommandLine GenerateFakeVpdCommand(
    const std::map<std::string, std::string>& contents,
    int exit_status = 0) {
  std::string shell_arg = kEchoCmd;
  shell_arg += " '";
  for (const auto& [key, value] : contents) {
    shell_arg +=
        base::StringPrintf(kMachineInfoFormat, key.c_str(), value.c_str());
  }

  shell_arg += "'; exit " + base::NumberToString(exit_status);

  return base::CommandLine({"/bin/sh", "-c", shell_arg});
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

  SourcesBuilder& set_vpd_tool(const base::CommandLine& tool_cmd) {
    sources_.vpd_tool = tool_cmd;
    return *this;
  }

  SourcesBuilder& set_machine_info(const base::FilePath& filepath) {
    sources_.machine_info_filepath = filepath;
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

    if (sources_.vpd_tool.GetProgram().empty()) {
      sources_.vpd_tool = base::CommandLine(base::FilePath(kFalseCmd));
    }

    if (sources_.machine_info_filepath.empty()) {
      sources_.machine_info_filepath = CreateFileInTempDir("", *temp_dir_);
    }

    if (sources_.oem_manifest_filepath.empty()) {
      sources_.oem_manifest_filepath = CreateFileInTempDir("", *temp_dir_);
    }

    if (sources_.cros_regions_filepath.empty()) {
      sources_.cros_regions_filepath = CreateFileInTempDir("", *temp_dir_);
    }

    return std::move(sources_);
  }

 private:
  const raw_ref<const base::ScopedTempDir> temp_dir_;
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

// Test that the provider loads statistics from VPD tool.
TEST_F(StatisticsProviderImplTest, LoadsVpdStatistics) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const auto fake_vpd_command = GenerateFakeVpdCommand({
      {"region", "nz"},
      {"ActivateDate", "2000-11"},
  });

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir()).set_vpd_tool(fake_vpd_command).Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  EXPECT_EQ(provider->GetMachineStatistic("region"), "nz");
  EXPECT_EQ(provider->GetMachineStatistic("ActivateDate"), "2000-11");

  // Check VPD status.
  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kValid);
}

// Test that the provider records correct status when all VPD contents are
// missing.
TEST_F(StatisticsProviderImplTest, RecordsErrorIfVpdFileIsMissing) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const auto fake_vpd_command =
      GenerateFakeVpdCommand({}, /*exit_status=*/DumpVpdExitCode::kBothInvalid);

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir()).set_vpd_tool(fake_vpd_command).Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Expect invalid VPD status because all VPD is missing.
  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kInvalid);
}

// Tests that StatisticsProvider generates stub statistics for VPD in a
// non-ChromeOS test environment.
TEST_F(StatisticsProviderImplTest, GeneratesStubVpdIfNotRunningChromeOS) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(
      kInvalidLsbReleaseContent, base::Time());
  ASSERT_FALSE(base::SysInfo::IsRunningOnChromeOS());

  const StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir())
          .Build();

  // Load statistics.
  auto provider =
      StatisticsProviderImpl::CreateProviderForTesting(testing_sources);
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  const auto initial_activate_date =
      provider->GetMachineStatistic(kActivateDateKey);
  EXPECT_TRUE(initial_activate_date);

  // Expect invalid VPD status because we're not ChromeOS.
  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kInvalid);

  // Current provider is going to be destroyed, copy its activate date.
  const std::string initial_activate_date_string =
      std::string(initial_activate_date.value_or(""));

  // Check fresh provider.
  provider = StatisticsProviderImpl::CreateProviderForTesting(testing_sources);
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Expect the same statistic as initial.
  EXPECT_EQ(provider->GetMachineStatistic(kActivateDateKey),
            initial_activate_date_string);

  // Still expect invalid VPD status.
  EXPECT_EQ(provider->GetVpdStatus(), StatisticsProvider::VpdStatus::kInvalid);
}

// Test that the provider returns correct VPD status with missing RO VPD.
TEST_F(StatisticsProviderImplTest, ReturnsInvalidRoVpdStatus) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const auto fake_vpd_command =
      GenerateFakeVpdCommand({{"ActivateDate", "2000-11"}},
                             /*exit_status=*/DumpVpdExitCode::kRoInvalid);

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir()).set_vpd_tool(fake_vpd_command).Build();

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

// Test that the provider returns correct VPD status with missing RW VPD.
TEST_F(StatisticsProviderImplTest, ReturnsInvalidRwVpd) {
  base::test::ScopedChromeOSVersionInfo scoped_version_info(kLsbReleaseContent,
                                                            base::Time());
  ASSERT_TRUE(base::SysInfo::IsRunningOnChromeOS());

  // Setup provider's sources.
  const auto fake_vpd_command =
      GenerateFakeVpdCommand({{"region", "nz"}},
                             /*exit_status=*/DumpVpdExitCode::kRwInvalid);

  StatisticsProviderImpl::StatisticsSources testing_sources =
      SourcesBuilder(temp_dir()).set_vpd_tool(fake_vpd_command).Build();

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
