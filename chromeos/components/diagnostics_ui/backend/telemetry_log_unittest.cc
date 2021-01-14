// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/telemetry_log.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chromeos/components/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace diagnostics {
namespace {

const char kNewline[] = "\n";

mojom::SystemInfoPtr CreateSystemInfoPtr(const std::string& board_name,
                                         const std::string& marketing_name,
                                         const std::string& cpu_model,
                                         uint32_t total_memory_kib,
                                         uint16_t cpu_threads_count,
                                         uint32_t cpu_max_clock_speed_khz,
                                         bool has_battery,
                                         const std::string& milestone_version) {
  auto version_info = mojom::VersionInfo::New(milestone_version);
  auto device_capabilities = mojom::DeviceCapabilities::New(has_battery);

  auto system_info = mojom::SystemInfo::New(
      board_name, marketing_name, cpu_model, total_memory_kib,
      cpu_threads_count, cpu_max_clock_speed_khz, std::move(version_info),
      std::move(device_capabilities));
  return system_info;
}

// Returns the lines of the log as a vector of strings.
std::vector<std::string> GetLogLines(const std::string& log) {
  return base::SplitString(log, kNewline,
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

}  // namespace

class TelemetryLogTest : public testing::Test {
 public:
  TelemetryLogTest() = default;

  ~TelemetryLogTest() override = default;
};

TEST_F(TelemetryLogTest, DetailedLogContents) {
  const std::string expected_board_name = "board_name";
  const std::string expected_marketing_name = "marketing_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const uint32_t expected_cpu_max_clock_speed_khz = 91011;
  const bool expected_has_battery = true;
  const std::string expected_milestone_version = "M99";

  mojom::SystemInfoPtr test_info = CreateSystemInfoPtr(
      expected_board_name, expected_marketing_name, expected_cpu_model,
      expected_total_memory_kib, expected_cpu_threads_count,
      expected_cpu_max_clock_speed_khz, expected_has_battery,
      expected_milestone_version);

  TelemetryLog log;

  log.UpdateSystemInfo(test_info.Clone());

  const std::string log_as_string = log.GetTelemetryLog();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);

  // Expect one title line and 8 content lines.
  EXPECT_EQ(9u, log_lines.size());

  EXPECT_EQ("Board Name: " + expected_board_name, log_lines[1]);
  EXPECT_EQ("Marketing Name: " + expected_marketing_name, log_lines[2]);
  EXPECT_EQ("CpuModel Name: " + expected_cpu_model, log_lines[3]);
  EXPECT_EQ(
      "Total Memory (kib): " + base::NumberToString(expected_total_memory_kib),
      log_lines[4]);
  EXPECT_EQ(
      "Thread Count:  " + base::NumberToString(expected_cpu_threads_count),
      log_lines[5]);
  EXPECT_EQ("Cpu Max Clock Speed (kHz):  " +
                base::NumberToString(expected_cpu_max_clock_speed_khz),
            log_lines[6]);
  EXPECT_EQ("Milestone Version: " + expected_milestone_version, log_lines[7]);
  EXPECT_EQ("Has Battery: " + base::NumberToString(expected_has_battery),
            log_lines[8]);
}

TEST_F(TelemetryLogTest, ChangeContents) {
  const std::string expected_board_name = "board_name";
  const std::string expected_marketing_name = "marketing_name";
  const std::string expected_cpu_model = "cpu_model";
  const uint32_t expected_total_memory_kib = 1234;
  const uint16_t expected_cpu_threads_count = 5678;
  const uint32_t expected_cpu_max_clock_speed_khz = 91011;
  const bool expected_has_battery = true;
  const std::string expected_milestone_version = "M99";

  mojom::SystemInfoPtr test_info = CreateSystemInfoPtr(
      expected_board_name, expected_marketing_name, expected_cpu_model,
      expected_total_memory_kib, expected_cpu_threads_count,
      expected_cpu_max_clock_speed_khz, expected_has_battery,
      expected_milestone_version);

  TelemetryLog log;

  log.UpdateSystemInfo(test_info.Clone());

  test_info->board_name = "new board_name";

  log.UpdateSystemInfo(test_info.Clone());

  const std::string log_as_string = log.GetTelemetryLog();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);

  EXPECT_EQ("Board Name: new board_name", log_lines[1]);
}

}  // namespace diagnostics
}  // namespace chromeos
