// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/diagnostics_ui/backend/routine_log.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_split.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/components/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace diagnostics {
namespace {

const char kLogFileName[] = "diagnostic_routine_log";
const char kSeparator[] = " - ";
const char kNewline[] = "\n";

// Returns the lines of the log as a vector of strings.
std::vector<std::string> GetLogLines(const std::string& log) {
  return base::SplitString(log, kNewline,
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

// Splits a single line of the log at `kSeparator`. It is expected that each log
// line contains exactly 3 components: 1) timestamp, 2) routine name, 3) status.
std::vector<std::string> GetLogLineContents(const std::string& log_line) {
  const std::vector<std::string> result = base::SplitString(
      log_line, kSeparator, base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  DCHECK_EQ(3u, result.size());
  return result;
}

}  // namespace

class RoutineLogTest : public testing::Test {
 public:
  RoutineLogTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.GetPath().AppendASCII(kLogFileName);
  }

  ~RoutineLogTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
  base::FilePath log_path_;
};

TEST_F(RoutineLogTest, Empty) {
  RoutineLog log(log_path_);

  EXPECT_FALSE(base::PathExists(log_path_));
}

TEST_F(RoutineLogTest, Basic) {
  RoutineLog log(log_path_);

  log.LogRoutineStarted(mojom::RoutineType::kCpuStress);

  EXPECT_TRUE(base::PathExists(log_path_));

  std::string contents;
  base::ReadFileToString(log_path_, &contents);
  const std::string first_line = GetLogLines(contents)[0];
  const std::vector<std::string> first_line_contents =
      GetLogLineContents(first_line);

  ASSERT_EQ(3u, first_line_contents.size());
  EXPECT_EQ("RoutineType::kCpuStress", first_line_contents[1]);
  EXPECT_EQ("Started", first_line_contents[2]);
}

TEST_F(RoutineLogTest, TwoLine) {
  RoutineLog log(log_path_);

  log.LogRoutineStarted(mojom::RoutineType::kMemory);
  log.LogRoutineCompleted(mojom::RoutineType::kMemory,
                          mojom::StandardRoutineResult::kTestPassed);
  EXPECT_TRUE(base::PathExists(log_path_));

  std::string contents;
  base::ReadFileToString(log_path_, &contents);
  const std::vector<std::string> log_lines = GetLogLines(contents);

  const std::string first_line = log_lines[0];
  const std::vector<std::string> first_line_contents =
      GetLogLineContents(first_line);

  ASSERT_EQ(3u, first_line_contents.size());
  EXPECT_EQ("RoutineType::kMemory", first_line_contents[1]);
  EXPECT_EQ("Started", first_line_contents[2]);

  const std::string second_line = log_lines[1];
  const std::vector<std::string> second_line_contents =
      GetLogLineContents(second_line);

  ASSERT_EQ(3u, second_line_contents.size());
  EXPECT_EQ("RoutineType::kMemory", second_line_contents[1]);
  EXPECT_EQ("StandardRoutineResult::kTestPassed", second_line_contents[2]);
}

}  // namespace diagnostics
}  // namespace chromeos
