// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/system_cpu/cpu_probe_linux.h"

#include <memory>
#include <optional>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "components/system_cpu/cpu_probe.h"
#include "components/system_cpu/cpu_sample.h"
#include "components/system_cpu/pressure_test_support.h"
#include "components/system_cpu/procfs_stat_cpu_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_cpu {

class CpuProbeLinuxTest : public testing::Test {
 public:
  CpuProbeLinuxTest() = default;

  ~CpuProbeLinuxTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    fake_stat_path_ = temp_dir_.GetPath().AppendASCII("stat");
    // Create the /proc/stat file before creating the parser, in case the parser
    // implementation keeps an open handle to the file indefinitely.
    stat_file_ = base::File(fake_stat_path_, base::File::FLAG_CREATE_ALWAYS |
                                                 base::File::FLAG_WRITE);

    probe_ =
        std::make_unique<FakePlatformCpuProbe<CpuProbeLinux>>(fake_stat_path_);
  }

  [[nodiscard]] bool WriteFakeStat(const std::string& contents) {
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
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath fake_stat_path_;
  base::File stat_file_;
  std::unique_ptr<FakePlatformCpuProbe<CpuProbeLinux>> probe_;
};

TEST_F(CpuProbeLinuxTest, ProductionDataNoCrash) {
  // Overwrite the fake probe with one that reads the production stat path.
  probe_ = std::make_unique<FakePlatformCpuProbe<CpuProbeLinux>>(
      base::FilePath(ProcfsStatCpuParser::kProcfsStatPath));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt)
      << "No baseline on first Update()";

  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  std::optional<CpuSample> sample = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_GE(sample->cpu_utilization, 0.0);
  EXPECT_LE(sample->cpu_utilization, 1.0);
}

TEST_F(CpuProbeLinuxTest, OneCoreFullInfo) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 0 0 0 0 0 0 0 0 0 0
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt)
      << "No baseline on first Update()";

  // user = 100, sys = 0, idle = 300
  // (user+sys) / (idle+user+sys) = 100/400
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 100 0 0 300 0 0 0 0 0 0
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  std::optional<CpuSample> sample = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_EQ(sample->cpu_utilization, 0.25);
}

TEST_F(CpuProbeLinuxTest, RepeatedSamples) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 0 0 0 0 0 0 0 0 0 0
)"));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt)
      << "No baseline on first Update()";

  // user = 100, sys = 0, idle = 300
  // (user+sys) / (idle+user+sys) = 100/400
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 100 0 0 300 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_EQ(sample->cpu_utilization, 0.25);

  // Second sample should have the difference since the first sample.
  // delta: user = 200-100, sys = 100-0, idle = 500-300
  // (user+sys) / (idle+user+sys) = 200/400
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 200 100 0 500 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample2 = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample2.has_value());
  EXPECT_EQ(sample2->cpu_utilization, 0.5);
}

TEST_F(CpuProbeLinuxTest, TwoCoresFullInfo) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 0 0 0 0 0 0 0 0 0 0
cpu1 0 0 0 0 0 0 0 0 0 0
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt)
      << "No baseline on first Update()";

  // cpu0: user = 100, sys = 0, idle = 300
  // (user+sys) / (idle+user+sys) = 100/400
  // cpu1: user = 100, sys = 100, idle = 200
  // (user+sys) / (idle+user+sys) = 200/400
  // total (user+sys) / total (idle+user+sys) = 300/800
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 100 0 0 300 0 0 0 0 0 0
cpu1 100 100 0 200 0 0 0 0 0 0
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  std::optional<CpuSample> sample = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_EQ(sample->cpu_utilization, 0.375);
}

TEST_F(CpuProbeLinuxTest, TwoCoresDifferentBaselines) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 10 20 0 30 0 0 0 0 0 0
cpu1 40 50 0 60 0 0 0 0 0 0
)"));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt)
      << "No baseline on first Update()";

  // cpu0 delta: user = 100, sys = 0, idle = 300
  // (user+sys) / (idle+user+sys) = 100/400
  // cpu1 delta: user = 100, sys = 100, idle = 200
  // (user+sys) / (idle+user+sys) = 200/400
  // total (user+sys) / total (idle+user+sys) = 300/800
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 110 20 0 330 0 0 0 0 0 0
cpu1 140 150 0 260 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_EQ(sample->cpu_utilization, 0.375);
}

TEST_F(CpuProbeLinuxTest, TwoCoresSecondCoreMissingStat) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 10 20 0 30 0 0 0 0 0 0
cpu1 40 50 0 60 0 0 0 0 0 0
intr 200 201 202 203 204 205 206 207 208 209 210 211 212 213 214 215 216 217
ctxt 23456789012
btime 1234567890
processes 6789012
procs_running 700
procs_blocked 600
softirq 900 901 902 903 904 905 906 907 908 909 910
)"));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt)
      << "No baseline on first Update()";

  // cpu0 delta: user = 100, sys = 0, idle = 300
  // (user+sys) / (idle+user+sys) = 100/400
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 110 20 0 330 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_EQ(sample->cpu_utilization, 0.25);

  // Second core reappears. Delta should be based on original baseline, not 0:
  // cpu0 delta: user = 100, sys = 0, idle = 300
  // (user+sys) / (idle+user+sys) = 100/400
  // cpu1 delta: user = 100, sys = 100, idle = 200
  // (user+sys) / (idle+user+sys) = 200/400
  // total (user+sys) / total (idle+user+sys) = 300/800
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 210 20 0 630 0 0 0 0 0 0
cpu1 140 150 0 260 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample2 = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample2.has_value());
  EXPECT_EQ(sample2->cpu_utilization, 0.375);
}

TEST_F(CpuProbeLinuxTest, TwoCoresSecondCoreAddedStat) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 10 20 0 30 0 0 0 0 0 0
)"));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt)
      << "No baseline on first Update()";

  // cpu0 delta: user = 100, sys = 0, idle = 300
  // (user+sys) / (idle+user+sys) = 100/400
  // cpu1 delta (vs 0): user = 100, sys = 100, idle = 200
  // (user+sys) / (idle+user+sys) = 200/400
  // But second core isn't included since it wasn't in the baseline, so:
  // total (user+sys) / total (idle+user+sys) = 100/400
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 110 20 0 330 0 0 0 0 0 0
cpu1 100 100 0 200 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_EQ(sample->cpu_utilization, 0.25);

  // cpu0 delta: user = 100, sys = 100, idle = 200
  // (user+sys) / (idle+user+sys) = 200/400
  // cpu1 delta: user = 100, sys = 0, idle = 300
  // (user+sys) / (idle+user+sys) = 100/400
  // Now the second core is included, with the last measurement as a baseline:
  // total (user+sys) / total (idle+user+sys) = 300/800
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 210 120 0 530 0 0 0 0 0 0
cpu1 200 100 0 500 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample2 = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample2.has_value());
  EXPECT_EQ(sample2->cpu_utilization, 0.375);
}

TEST_F(CpuProbeLinuxTest, ParseErrorInBaseline) {
  ASSERT_TRUE(WriteFakeStat(R"(
parse error
)"));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt)
      << "No baseline on first Update()";

  // Update should be ignored as baseline is unreliable.
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 100 0 0 300 0 0 0 0 0 0
)"));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt);

  // Next update can use the last update as a baseline.
  // cpu0 delta: user = 100, sys = 0, idle = 300
  // (user+sys) / (idle+user+sys) = 100/400
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 200 0 0 600 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_EQ(sample->cpu_utilization, 0.25);
}

TEST_F(CpuProbeLinuxTest, ParseErrorInSample) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 10 20 0 30 0 0 0 0 0 0
)"));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt)
      << "No baseline on first Update()";

  ASSERT_TRUE(WriteFakeStat(R"(
bad stat file
)"));
  EXPECT_FALSE(probe_->UpdateAndWaitForSample().has_value());

  // Second sample should have the difference since the baseline, since the
  // first sample was ignored.
  // delta: user = 100, sys = 100, idle = 200
  // (user+sys) / (idle+user+sys) = 200/400
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 110 120 0 230 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample2 = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample2.has_value());
  EXPECT_EQ(sample2->cpu_utilization, 0.5);
}

TEST_F(CpuProbeLinuxTest, ZeroAndNegativeDeltas) {
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 10 20 0 30 0 0 0 0 0 0
cpu1 40 50 0 60 0 0 0 0 0 0
)"));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt)
      << "No baseline on first Update()";

  // cpu0: active delta 0, idle delta 100 (0% usage)
  // cpu1: active delta 100, idle delta 0 (100% usage)
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 10 20 0 130 0 0 0 0 0 0
cpu1 140 50 0 60 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_EQ(sample->cpu_utilization, 0.5);

  // cpu0: active delta 0, idle delta 100 (0% usage)
  // cpu1: active delta 0, idle delta 0 (no time passed - ignore)
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 10 20 0 230 0 0 0 0 0 0
cpu1 140 50 0 60 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample2 = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample2.has_value());
  EXPECT_EQ(sample2->cpu_utilization, 0.0);

  // Both cores: active delta 0, idle delta 0 (no time passed - ignore)
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 10 20 0 230 0 0 0 0 0 0
cpu1 140 50 0 60 0 0 0 0 0 0
)"));
  EXPECT_EQ(probe_->UpdateAndWaitForSample(), std::nullopt);

  // cpu0: user time increases, sys decreases - ignore only the decreasing
  // values, include the others.
  // cpu0 delta: user = 100, sys = -10 (clamped to 0), idle = 300
  // (user+sys) / (idle+user+sys) = 100/400
  // cpu1: all times decrease - ignore this core completely.
  ASSERT_TRUE(WriteFakeStat(R"(
cpu 0 0 0 0 0 0 0 0 0 0
cpu0 110 10 0 530 0 0 0 0 0 0
cpu1 130 40 0 50 0 0 0 0 0 0
)"));
  std::optional<CpuSample> sample3 = probe_->UpdateAndWaitForSample();
  ASSERT_TRUE(sample3.has_value());
  EXPECT_EQ(sample3->cpu_utilization, 0.25);
}

}  // namespace system_cpu
