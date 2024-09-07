// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/system_cpu/procfs_stat_cpu_parser.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_cpu {

class ProcfsStatCpuParserTest : public testing::Test {
 public:
  ~ProcfsStatCpuParserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    fake_stat_path_ = temp_dir_.GetPath().AppendASCII("stat");
    // Create the /proc/stat file before creating the parser, in case the parser
    // implementation keeps an open handle to the file indefinitely.
    stat_file_ = base::File(fake_stat_path_, base::File::FLAG_CREATE_ALWAYS |
                                                 base::File::FLAG_WRITE);
    parser_ = std::make_unique<ProcfsStatCpuParser>(fake_stat_path_);
  }

  [[nodiscard]] bool WriteFakeStat(std::string_view contents) {
    if (!stat_file_.SetLength(0)) {
      return false;
    }
    if (contents.size() > 0) {
      if (!stat_file_.Write(0, contents.data(), contents.size())) {
        return false;
      }
    }
    return true;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath fake_stat_path_;
  base::File stat_file_;
  std::unique_ptr<ProcfsStatCpuParser> parser_;
};

TEST_F(ProcfsStatCpuParserTest, ProductionDataNoCrash) {
  base::FilePath production_path(ProcfsStatCpuParser::kProcfsStatPath);
  ProcfsStatCpuParser parser(production_path);
  bool parse_success = parser.Update();
  if (!parse_success) {
    // Log /proc/stat for debugging.
    std::string stat_file = "(failed to read /proc/stat)";
    base::ReadFileToString(production_path, &stat_file);
    FAIL() << stat_file;
  }
}

TEST_F(ProcfsStatCpuParserTest, MissingFile) {
  stat_file_ = base::File();
  ASSERT_TRUE(base::DeleteFile(fake_stat_path_));
  ProcfsStatCpuParser parser(fake_stat_path_);

  EXPECT_FALSE(parser.Update());
  EXPECT_EQ(parser_->core_times().size(), 0u);
}

TEST_F(ProcfsStatCpuParserTest, EmptyFile) {
  ASSERT_TRUE(WriteFakeStat(""));
  EXPECT_EQ(0, stat_file_.GetLength()) << "Incorrect empty file";
  EXPECT_TRUE(parser_->Update());

  EXPECT_EQ(parser_->core_times().size(), 0u);
}

TEST_F(ProcfsStatCpuParserTest, SingleCoreSingleDigit) {
  // Zero is not used as a value, because it is the initial value of CoreTimes
  // members. Avoiding that value brings assurance that Update() modified the
  // CoreTimes member.
  ASSERT_TRUE(WriteFakeStat("cpu0 1 2 3 4 5 6 7 8 9 1"));
  EXPECT_TRUE(parser_->Update());

  ASSERT_EQ(parser_->core_times().size(), 1u);
  EXPECT_EQ(parser_->core_times()[0].user(), 1u);
  EXPECT_EQ(parser_->core_times()[0].nice(), 2u);
  EXPECT_EQ(parser_->core_times()[0].system(), 3u);
  EXPECT_EQ(parser_->core_times()[0].idle(), 4u);
  EXPECT_EQ(parser_->core_times()[0].iowait(), 5u);
  EXPECT_EQ(parser_->core_times()[0].irq(), 6u);
  EXPECT_EQ(parser_->core_times()[0].softirq(), 7u);
  EXPECT_EQ(parser_->core_times()[0].steal(), 8u);
  EXPECT_EQ(parser_->core_times()[0].guest(), 9u);
  EXPECT_EQ(parser_->core_times()[0].guest_nice(), 1u);
}

TEST_F(ProcfsStatCpuParserTest, SingleCoreMultipleDigits) {
  // Checks that the number parser can parse numbers of various sizes. The sizes
  // are not in ascending order, to make sure that the number size isn't in sync
  // with a loop counter.
  //
  // Number of digits in each number: 2 4 3 6 5 8 7 9 1 10
  ASSERT_TRUE(WriteFakeStat(
      "cpu0 12 3456 789 102345 67890 12345678 9012345 678901234 5 1234567890"));
  EXPECT_TRUE(parser_->Update());

  ASSERT_EQ(parser_->core_times().size(), 1u);
  EXPECT_EQ(parser_->core_times()[0].user(), 12u);
  EXPECT_EQ(parser_->core_times()[0].nice(), 3456u);
  EXPECT_EQ(parser_->core_times()[0].system(), 789u);
  EXPECT_EQ(parser_->core_times()[0].idle(), 102345u);
  EXPECT_EQ(parser_->core_times()[0].iowait(), 67890u);
  EXPECT_EQ(parser_->core_times()[0].irq(), 12345678u);
  EXPECT_EQ(parser_->core_times()[0].softirq(), 9012345u);
  EXPECT_EQ(parser_->core_times()[0].steal(), 678901234u);
  EXPECT_EQ(parser_->core_times()[0].guest(), 5u);
  EXPECT_EQ(parser_->core_times()[0].guest_nice(), 1234567890u);
}

TEST_F(ProcfsStatCpuParserTest, SingleCoreTooManyNumbers) {
  ASSERT_TRUE(WriteFakeStat("cpu0 10 11 12 13 14 15 16 17 18 19 20 21 22 23"));
  EXPECT_TRUE(parser_->Update());

  ASSERT_EQ(parser_->core_times().size(), 1u);
  EXPECT_EQ(parser_->core_times()[0].user(), 10u);
  EXPECT_EQ(parser_->core_times()[0].nice(), 11u);
  EXPECT_EQ(parser_->core_times()[0].system(), 12u);
  EXPECT_EQ(parser_->core_times()[0].idle(), 13u);
  EXPECT_EQ(parser_->core_times()[0].iowait(), 14u);
  EXPECT_EQ(parser_->core_times()[0].irq(), 15u);
  EXPECT_EQ(parser_->core_times()[0].softirq(), 16u);
  EXPECT_EQ(parser_->core_times()[0].steal(), 17u);
  EXPECT_EQ(parser_->core_times()[0].guest(), 18u);
  EXPECT_EQ(parser_->core_times()[0].guest_nice(), 19u);
}

TEST_F(ProcfsStatCpuParserTest, SingleCoreTooFewNumbers) {
  const std::vector<const char*> test_cases = {
      "cpu0 1 2 3 4 5 6 7 8 9",
      "cpu0 1 2 3 4 5 6 7 8",
      "cpu0 1 2 3 4 5 6 7",
      "cpu0 1 2 3 4 5 6",
      "cpu0 1 2 3 4 5",
      "cpu0 1 2 3 4",
      "cpu0 1 2 3",
      "cpu0 1 2",
      "cpu0 1",
  };

  for (const char* test_case : test_cases) {
    SCOPED_TRACE(test_case);

    ProcfsStatCpuParser parser(fake_stat_path_);
    ASSERT_TRUE(WriteFakeStat(test_case));
    EXPECT_TRUE(parser.Update());

    ASSERT_EQ(parser.core_times().size(), 1u);
    EXPECT_EQ(parser.core_times()[0].user(), 0u);
    EXPECT_EQ(parser.core_times()[0].nice(), 0u);
    EXPECT_EQ(parser.core_times()[0].system(), 0u);
    EXPECT_EQ(parser.core_times()[0].idle(), 0u);
    EXPECT_EQ(parser.core_times()[0].iowait(), 0u);
    EXPECT_EQ(parser.core_times()[0].irq(), 0u);
    EXPECT_EQ(parser.core_times()[0].softirq(), 0u);
    EXPECT_EQ(parser.core_times()[0].steal(), 0u);
    EXPECT_EQ(parser.core_times()[0].guest(), 0u);
    EXPECT_EQ(parser.core_times()[0].guest_nice(), 0u);
  }
}

TEST_F(ProcfsStatCpuParserTest, SingleCoreNoNumbers) {
  ASSERT_TRUE(WriteFakeStat("cpu0"));
  EXPECT_TRUE(parser_->Update());

  EXPECT_EQ(parser_->core_times().size(), 0u);
}

TEST_F(ProcfsStatCpuParserTest, IncorrectCoreSpecifier) {
  const std::vector<const char*> test_cases = {
      "dpu0 1 2 3 4 5 6 7 8 9 0",
      "cru0 1 2 3 4 5 6 7 8 9 0",
      "cpv0 1 2 3 4 5 6 7 8 9 0",
      "cpu 1 2 3 4 5 6 7 8 9 0",
      "cpua 1 2 3 4 5 6 7 8 9 0",
      "cpu- 1 2 3 4 5 6 7 8 9 0",
      "cpu-1 1 2 3 4 5 6 7 8 9 0",
      "cpu-100 1 2 3 4 5 6 7 8 9 0",
      "cpu1a 1 2 3 4 5 6 7 8 9 0",
      "cpu1- 1 2 3 4 5 6 7 8 9 0",
      "cpu\U0001f41b 1 2 3 4 5 6 7 8 9 0",
      "cpu1\U0001f41b 1 2 3 4 5 6 7 8 9 0",
      "cpu\U0001f41b1 1 2 3 4 5 6 7 8 9 0",
  };

  for (const char* test_case : test_cases) {
    SCOPED_TRACE(test_case);

    ProcfsStatCpuParser parser(fake_stat_path_);
    ASSERT_TRUE(WriteFakeStat(test_case));
    EXPECT_TRUE(parser.Update());

    EXPECT_EQ(parser.core_times().size(), 0u);
  }
}

TEST_F(ProcfsStatCpuParserTest, InvalidFirstNumber) {
  const std::vector<const char*> test_cases = {
      "cpu0 a 2 3 4 5 6 7 8 9 10",
      "cpu0 - 2 3 4 5 6 7 8 9 10",
      "cpu0 -1 2 3 4 5 6 7 8 9 10",
      "cpu0 -100 2 3 4 5 6 7 8 9 10",
      "cpu0 1a 2 3 4 5 6 7 8 9 10",
      "cpu0 1234a 2 3 4 5 6 7 8 9 10",
      "cpu0 123456789012345678901 2 3 4 5 6 7 8 9 10",
      "cpu0 -123456789012345678901 2 3 4 5 6 7 8 9 10",
      "cpu0 18446744073709551616 2 3 4 5 6 7 8 9 10",
      "cpu0 \U0001f41b 2 3 4 5 6 7 8 9 10",
      "cpu0 1\U0001f41b 2 3 4 5 6 7 8 9 10",
      "cpu0 \U0001f41b1 2 3 4 5 6 7 8 9 10",
  };

  for (const char* test_case : test_cases) {
    SCOPED_TRACE(test_case);

    ProcfsStatCpuParser parser(fake_stat_path_);
    ASSERT_TRUE(WriteFakeStat(test_case));
    EXPECT_TRUE(parser.Update());

    ASSERT_EQ(parser.core_times().size(), 1u);
    EXPECT_EQ(parser.core_times()[0].user(), 0u);
    EXPECT_EQ(parser.core_times()[0].nice(), 0u);
    EXPECT_EQ(parser.core_times()[0].system(), 0u);
    EXPECT_EQ(parser.core_times()[0].idle(), 0u);
    EXPECT_EQ(parser.core_times()[0].iowait(), 0u);
    EXPECT_EQ(parser.core_times()[0].irq(), 0u);
    EXPECT_EQ(parser.core_times()[0].softirq(), 0u);
    EXPECT_EQ(parser.core_times()[0].steal(), 0u);
    EXPECT_EQ(parser.core_times()[0].guest(), 0u);
    EXPECT_EQ(parser.core_times()[0].guest_nice(), 0u);
  }
}

TEST_F(ProcfsStatCpuParserTest, InvalidNumberSkipped) {
  struct TestCase {
    const char* line;
    int invalid_index;
  };
  const std::vector<TestCase> test_cases = {
      {"cpu0 1 2 3 4 5 6 7 8 9 10", 10},
      {"cpu0 a 2 3 4 5 6 7 8 9 10", 0},
      {"cpu0 1 a 3 4 5 6 7 8 9 10", 1},
      {"cpu0 1 2 a 4 5 6 7 8 9 10", 2},
      {"cpu0 1 2 3 a 5 6 7 8 9 10", 3},
      {"cpu0 1 2 3 4 a 6 7 8 9 10", 4},
      {"cpu0 1 2 3 4 5 a 7 8 9 10", 5},
      {"cpu0 1 2 3 4 5 6 a 8 9 10", 6},
      {"cpu0 1 2 3 4 5 6 7 a 9 10", 7},
      {"cpu0 1 2 3 4 5 6 7 8 a 10", 8},
      {"cpu0 1 2 3 4 5 6 7 8 9 a", 9},
      {"cpu0 1 \U0001f41b 3 4 5 6 7 8 9 10", 1},
      {"cpu0 1 2 3\U0001f41b 4 5 6 7 8 9 10", 2},
      {"cpu0 1 2 3 \U0001f41b4 5 6 7 8 9 10", 3},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.line);

    ProcfsStatCpuParser parser(fake_stat_path_);
    ASSERT_TRUE(WriteFakeStat(test_case.line));
    EXPECT_TRUE(parser.Update());

    ASSERT_EQ(parser.core_times().size(), 1u);

    EXPECT_EQ(parser.core_times()[0].user(),
              (test_case.invalid_index <= 0) ? 0u : 1u);
    EXPECT_EQ(parser.core_times()[0].nice(),
              (test_case.invalid_index <= 1) ? 0u : 2u);
    EXPECT_EQ(parser.core_times()[0].system(),
              (test_case.invalid_index <= 2) ? 0u : 3u);
    EXPECT_EQ(parser.core_times()[0].idle(),
              (test_case.invalid_index <= 3) ? 0u : 4u);
    EXPECT_EQ(parser.core_times()[0].iowait(),
              (test_case.invalid_index <= 4) ? 0u : 5u);
    EXPECT_EQ(parser.core_times()[0].irq(),
              (test_case.invalid_index <= 5) ? 0u : 6u);
    EXPECT_EQ(parser.core_times()[0].softirq(),
              (test_case.invalid_index <= 6) ? 0u : 7u);
    EXPECT_EQ(parser.core_times()[0].steal(),
              (test_case.invalid_index <= 7) ? 0u : 8u);
    EXPECT_EQ(parser.core_times()[0].guest(),
              (test_case.invalid_index <= 8) ? 0u : 9u);
    EXPECT_EQ(parser.core_times()[0].guest_nice(),
              (test_case.invalid_index <= 9) ? 0u : 10u);
  }
}

TEST_F(ProcfsStatCpuParserTest, SingleCoreIgnoresCounterDecrease) {
  struct TestCase {
    const char* line;
    int invalid_index;
  };
  const std::vector<TestCase> test_cases = {
      {"cpu0 106 116 126 136 146 156 166 176 186 196", 10},
      {"cpu0 104 116 126 136 146 156 166 176 186 196", 0},
      {"cpu0 106 114 126 136 146 156 166 176 186 196", 1},
      {"cpu0 106 116 124 136 146 156 166 176 186 196", 2},
      {"cpu0 106 116 126 134 146 156 166 176 186 196", 3},
      {"cpu0 106 116 126 136 144 156 166 176 186 196", 4},
      {"cpu0 106 116 126 136 146 154 166 176 186 196", 5},
      {"cpu0 106 116 126 136 146 156 164 176 186 196", 6},
      {"cpu0 106 116 126 136 146 156 166 174 186 196", 7},
      {"cpu0 106 116 126 136 146 156 166 176 184 196", 8},
      {"cpu0 106 116 126 136 146 156 166 176 186 194", 9},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.line);

    ProcfsStatCpuParser parser(fake_stat_path_);
    ASSERT_TRUE(WriteFakeStat("cpu0 105 115 125 135 145 155 165 175 185 195"));
    EXPECT_TRUE(parser.Update());
    ASSERT_EQ(parser.core_times().size(), 1u);

    ASSERT_TRUE(WriteFakeStat(test_case.line));
    EXPECT_TRUE(parser.Update());

    EXPECT_EQ(parser.core_times()[0].user(),
              (test_case.invalid_index == 0) ? 105u : 106u);
    EXPECT_EQ(parser.core_times()[0].nice(),
              (test_case.invalid_index == 1) ? 115u : 116u);
    EXPECT_EQ(parser.core_times()[0].system(),
              (test_case.invalid_index == 2) ? 125u : 126u);
    EXPECT_EQ(parser.core_times()[0].idle(),
              (test_case.invalid_index == 3) ? 135u : 136u);
    EXPECT_EQ(parser.core_times()[0].iowait(),
              (test_case.invalid_index == 4) ? 145u : 146u);
    EXPECT_EQ(parser.core_times()[0].irq(),
              (test_case.invalid_index == 5) ? 155u : 156u);
    EXPECT_EQ(parser.core_times()[0].softirq(),
              (test_case.invalid_index == 6) ? 165u : 166u);
    EXPECT_EQ(parser.core_times()[0].steal(),
              (test_case.invalid_index == 7) ? 175u : 176u);
    EXPECT_EQ(parser.core_times()[0].guest(),
              (test_case.invalid_index == 8) ? 185u : 186u);
    EXPECT_EQ(parser.core_times()[0].guest_nice(),
              (test_case.invalid_index == 9) ? 195u : 196u);
  }
}

TEST_F(ProcfsStatCpuParserTest, MissingCores) {
  ASSERT_TRUE(WriteFakeStat("cpu5 1 2 3 4 5 6 7 8 9 1"));
  EXPECT_TRUE(parser_->Update());

  ASSERT_EQ(parser_->core_times().size(), 6u);

  EXPECT_EQ(parser_->core_times()[5].user(), 1u);
  EXPECT_EQ(parser_->core_times()[5].nice(), 2u);
  EXPECT_EQ(parser_->core_times()[5].system(), 3u);
  EXPECT_EQ(parser_->core_times()[5].idle(), 4u);
  EXPECT_EQ(parser_->core_times()[5].iowait(), 5u);
  EXPECT_EQ(parser_->core_times()[5].irq(), 6u);
  EXPECT_EQ(parser_->core_times()[5].softirq(), 7u);
  EXPECT_EQ(parser_->core_times()[5].steal(), 8u);
  EXPECT_EQ(parser_->core_times()[5].guest(), 9u);
  EXPECT_EQ(parser_->core_times()[5].guest_nice(), 1u);

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(parser_->core_times()[i].user(), 0u);
    EXPECT_EQ(parser_->core_times()[i].nice(), 0u);
    EXPECT_EQ(parser_->core_times()[i].system(), 0u);
    EXPECT_EQ(parser_->core_times()[i].idle(), 0u);
    EXPECT_EQ(parser_->core_times()[i].iowait(), 0u);
    EXPECT_EQ(parser_->core_times()[i].irq(), 0u);
    EXPECT_EQ(parser_->core_times()[i].softirq(), 0u);
    EXPECT_EQ(parser_->core_times()[i].steal(), 0u);
    EXPECT_EQ(parser_->core_times()[i].guest(), 0u);
    EXPECT_EQ(parser_->core_times()[i].guest_nice(), 0u);
  }
}

TEST_F(ProcfsStatCpuParserTest, MultipleCores) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 100 101 102 103 104 105 106 107 108 109
cpu2 30 31 32 33 34 35 36 37 38 39
cpu3 40 41 42 43 44 45 46 47 48 49
cpu0 10 11 12 13 14 15 16 17 18 19
cpu5 60 61 62 63 64 65 66 67 68 69
cpu1 20 21 22 23 24 25 26 27 28 29
cpu4 50 51 52 53 54 55 56 57 58 59
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  EXPECT_TRUE(parser_->Update());

  ASSERT_EQ(parser_->core_times().size(), 6u);

  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(parser_->core_times()[i].user(),
              static_cast<uint64_t>(i * 10 + 10));
    EXPECT_EQ(parser_->core_times()[i].nice(),
              static_cast<uint64_t>(i * 10 + 11));
    EXPECT_EQ(parser_->core_times()[i].system(),
              static_cast<uint64_t>(i * 10 + 12));
    EXPECT_EQ(parser_->core_times()[i].idle(),
              static_cast<uint64_t>(i * 10 + 13));
    EXPECT_EQ(parser_->core_times()[i].iowait(),
              static_cast<uint64_t>(i * 10 + 14));
    EXPECT_EQ(parser_->core_times()[i].irq(),
              static_cast<uint64_t>(i * 10 + 15));
    EXPECT_EQ(parser_->core_times()[i].softirq(),
              static_cast<uint64_t>(i * 10 + 16));
    EXPECT_EQ(parser_->core_times()[i].steal(),
              static_cast<uint64_t>(i * 10 + 17));
    EXPECT_EQ(parser_->core_times()[i].guest(),
              static_cast<uint64_t>(i * 10 + 18));
    EXPECT_EQ(parser_->core_times()[i].guest_nice(),
              static_cast<uint64_t>(i * 10 + 19));
  }
}

}  // namespace system_cpu
