// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_cpu/cpu_freq_android.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace system_cpu {

class TestDelegate : public CPUFreqMonitor::Delegate {
 public:
  explicit TestDelegate(const std::string& temp_dir_path)
      : temp_dir_path_(temp_dir_path) {}

  void set_cpu_ids(const std::vector<CPUFreqMonitor::CpuId>& cpu_ids) {
    cpu_ids_ = cpu_ids;
  }

  void set_kernel_max_cpu(CPUFreqMonitor::CpuId kernel_max_cpu) {
    kernel_max_cpu_ = kernel_max_cpu;
  }

  // CPUFreqMonitor::Delegate implementation:
  std::vector<CPUFreqMonitor::CpuId> GetCPUIds() const override {
    // Use the test values if available.
    if (cpu_ids_.size() > 0) {
      return cpu_ids_;
    }
    // Otherwise fall back to the original function.
    return CPUFreqMonitor::Delegate::GetCPUIds();
  }

  CPUFreqMonitor::CpuId GetKernelMaxCPUId() const override {
    return kernel_max_cpu_;
  }

  std::string GetScalingCurFreqPathString(
      CPUFreqMonitor::CpuId cpu_id) const override {
    return absl::StrFormat("%s/scaling_cur_freq%d", temp_dir_path_.c_str(),
                           cpu_id.value());
  }

  std::string GetRelatedCPUsPathString(
      CPUFreqMonitor::CpuId cpu_id) const override {
    return absl::StrFormat("%s/related_cpus%d", temp_dir_path_.c_str(),
                           cpu_id.value());
  }

 private:
  std::vector<CPUFreqMonitor::CpuId> cpu_ids_;

  std::string temp_dir_path_;
  CPUFreqMonitor::CpuId kernel_max_cpu_{0};
};

class CPUFreqMonitorTest : public testing::Test {
 public:
  CPUFreqMonitorTest() = default;

  void SetUp() override {
    temp_dir_ = std::make_unique<base::ScopedTempDir>();
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());

    std::string base_path = temp_dir_->GetPath().value();
    owned_delegate_ = std::make_unique<TestDelegate>(base_path);
    // Retain a pointer to the delegate since we're passing ownership to the
    // monitor but we need to be able to modify it.
    delegate_ = owned_delegate_.get();
  }

  void TearDown() override { temp_dir_.reset(); }

  void CreateDefaultScalingCurFreqFiles(
      const std::vector<CPUFreqMonitor::CoreFrequency>& frequencies) {
    for (auto& [id, freq] : frequencies) {
      std::string file_path = delegate_->GetScalingCurFreqPathString(id);
      std::string str_freq = absl::StrFormat("%d\n", freq);
      base::WriteFile(base::FilePath(file_path), str_freq);
    }
  }

  void CreateRelatedCPUFiles(const std::vector<unsigned int>& clusters,
                             const std::vector<std::string>& related_cpus) {
    for (unsigned int i = 0; i < clusters.size(); i++) {
      base::WriteFile(base::FilePath(delegate_->GetRelatedCPUsPathString(
                          CPUFreqMonitor::CpuId(i))),
                      related_cpus[clusters[i]]);
    }
  }

  base::ScopedTempDir* temp_dir() { return temp_dir_.get(); }
  TestDelegate* delegate() { return delegate_; }

 protected:
  std::unique_ptr<base::ScopedTempDir> temp_dir_;
  raw_ptr<TestDelegate> delegate_;
  std::unique_ptr<TestDelegate> owned_delegate_;
};

TEST_F(CPUFreqMonitorTest, TestSample) {
  // Vector of CPU ID to frequency.
  std::vector<CPUFreqMonitor::CoreFrequency> frequencies = {
      {CPUFreqMonitor::CpuId(0), 500}, {CPUFreqMonitor::CpuId(4), 1000}};
  std::vector<CPUFreqMonitor::CpuId> cpu_ids;
  for (auto& pair : frequencies) {
    cpu_ids.push_back(pair.core_id);
  }
  delegate()->set_cpu_ids(cpu_ids);

  // Build some files with CPU frequency info in it to sample.
  std::vector<std::pair<CPUFreqMonitor::CpuId, base::ScopedFD>> fds;
  for (auto& pair : frequencies) {
    std::string file_path =
        absl::StrFormat("%s/temp%d", temp_dir()->GetPath().value().c_str(),
                        pair.core_id.value());

    // Uses raw file descriptors so we can build our ScopedFDs in the same loop.
    int fd = open(file_path.c_str(), O_RDWR | O_CREAT | O_SYNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    ASSERT_FALSE(fd == -1);

    std::string str_freq = absl::StrFormat("%d\n", pair.freq);
    ssize_t result = write(fd, str_freq.c_str(), str_freq.length());
    ASSERT_EQ(result, static_cast<ssize_t>(str_freq.length()));
    fds.emplace_back(pair.core_id, base::ScopedFD(fd));
  }

  CreateDefaultScalingCurFreqFiles(frequencies);

  auto monitor = std::make_unique<CPUFreqMonitor>(std::move(owned_delegate_));

  auto recorded_freqs = monitor->GetCoreFrequencies();

  ASSERT_EQ(recorded_freqs.size(), frequencies.size());
  for (unsigned int i = 0; i < frequencies.size(); i++) {
    ASSERT_EQ(frequencies[i].core_id, recorded_freqs[i].core_id);
    ASSERT_EQ(frequencies[i].freq, recorded_freqs[i].freq);
  }
}

TEST_F(CPUFreqMonitorTest, TestDelegate_GetCPUIds) {
  delegate()->set_kernel_max_cpu(CPUFreqMonitor::CpuId(8));
  std::vector<std::string> related_cpus = {"0 1 2 3\n", "4 5 6 7\n"};
  std::vector<unsigned int> clusters = {0, 0, 0, 0, 1, 1, 1, 1};

  CreateRelatedCPUFiles(clusters, related_cpus);

  std::vector<CPUFreqMonitor::CpuId> cpu_ids = delegate()->GetCPUIds();
  ASSERT_EQ(cpu_ids.size(), 2U);
  EXPECT_EQ(cpu_ids[0], CPUFreqMonitor::CpuId(0U));
  EXPECT_EQ(cpu_ids[1], CPUFreqMonitor::CpuId(4U));
}

TEST_F(CPUFreqMonitorTest, TestDelegate_GetCPUIds_FailReadingFallback) {
  delegate()->set_kernel_max_cpu(CPUFreqMonitor::CpuId(8));

  // Relies on GetRelatedCPUsPathString() test override.
  std::vector<CPUFreqMonitor::CpuId> cpu_ids = delegate()->GetCPUIds();
  ASSERT_EQ(cpu_ids.size(), 1U);
  EXPECT_EQ(cpu_ids[0], CPUFreqMonitor::CpuId(0U));
}

}  // namespace system_cpu
