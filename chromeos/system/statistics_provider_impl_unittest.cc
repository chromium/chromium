// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/statistics_provider_impl.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::system {

namespace {

// Echo is used to fake crossystem tool.
constexpr char kEchoCmd[] = "/bin/echo";
constexpr char kLsbReleaseContent[] = "CHROMEOS_RELEASE_NAME=Chromium OS\n";
constexpr char kInvalidLsbReleaseContent[] = "Just empty";

constexpr char kCrossystemToolFormat[] = "%s = %s   # %s\n";
constexpr char kMachineInfoFormat[] = "\"%s\"=\"%s\"\n";

// Creates a file with unique name in the temp dir and fills it with
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

  SourcesBuilder& set_crossystem_tool(const std::string& tool_cmd,
                                      const std::string& tool_args) {
    EXPECT_FALSE(tool_args.empty());
    sources_.crossystem_tool = {tool_cmd, tool_args};
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

  SourcesBuilder& set_oem_manifest(const base::FilePath& filepath) {
    sources_.oem_manifest_filepath = filepath;
    return *this;
  }

  SourcesBuilder& set_cros_regions(const base::FilePath& filepath) {
    sources_.cros_regions_filepath = filepath;
    return *this;
  }

  StatisticsProviderImpl::StatisticsSources Build() {
    if (sources_.crossystem_tool.empty()) {
      sources_.crossystem_tool = {kEchoCmd};
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
          .set_crossystem_tool(kEchoCmd, kEchoArgs)
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  {
    std::string result;
    EXPECT_TRUE(provider->GetMachineStatistic("crossystem_key_1", &result));
    EXPECT_EQ(result, "crossystem_value_1");
  }

  {
    std::string result;
    EXPECT_TRUE(provider->GetMachineStatistic("crossystem_key_2", &result));
    EXPECT_EQ(result, "crossystem_value_2");
  }

  {
    std::string result;
    EXPECT_FALSE(provider->GetMachineStatistic("crossystem_key_3", &result));
    EXPECT_TRUE(result.empty()) << "Unexpected value loaded: " << result;
  }

  {
    std::string result;
    EXPECT_FALSE(provider->GetMachineStatistic("crossystem_key_4", &result));
    EXPECT_TRUE(result.empty()) << "Unexpected value loaded: " << result;
  }

  {
    std::string result;
    EXPECT_FALSE(provider->GetMachineStatistic("crossystem_key_5", &result));
    EXPECT_TRUE(result.empty()) << "Unexpected value loaded: " << result;
  }
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
          .set_crossystem_tool(kEchoCmd, kEchoArgs)
          .set_machine_info(
              CreateFileInTempDir(kMachineInfoStatistics, temp_dir()))
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  {
    std::string result;
    EXPECT_TRUE(provider->GetMachineStatistic(kFirmwareWriteProtectCurrentKey,
                                              &result));
    EXPECT_EQ(result, "machine_info_value");
  }
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
          .set_crossystem_tool(kEchoCmd, kEchoArgs)
          .set_machine_info(
              CreateFileInTempDir(kMachineInfoStatistics, temp_dir()))
          .Build();

  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  {
    std::string result;
    EXPECT_TRUE(provider->GetMachineStatistic(kFirmwareWriteProtectCurrentKey,
                                              &result));
    EXPECT_EQ(result, "crossytem_value");
  }
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
          .set_crossystem_tool(kEchoCmd, kEchoArgs)
          .Build();

  // Load statistics.
  auto provider = StatisticsProviderImpl::CreateProviderForTesting(
      std::move(testing_sources));
  LoadStatistics(provider.get(), /*load_oem_manifest=*/false);

  // Check statistics.
  {
    std::string result;
    EXPECT_FALSE(provider->GetMachineStatistic("crossystem_key_1", &result));
    EXPECT_TRUE(result.empty()) << "Unexpected value loaded: " << result;
  }

  {
    std::string result;
    EXPECT_FALSE(provider->GetMachineStatistic("crossystem_key_2", &result));
    EXPECT_TRUE(result.empty()) << "Unexpected value loaded: " << result;
  }
}

}  // namespace chromeos::system
