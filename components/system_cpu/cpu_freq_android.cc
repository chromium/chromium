// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/system_cpu/cpu_freq_android.h"

#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/fixed_array.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace system_cpu {

std::vector<CPUFreqMonitor::CpuId> CPUFreqMonitor::Delegate::GetCPUIds() const {
  std::vector<CpuId> result;
  CpuId kernel_max_cpu = GetKernelMaxCPUId();
  // CPUs related to one that's already marked for monitoring get set to "false"
  // so we don't needlessly monitor CPUs with redundant frequency information.
  base::FixedArray<bool> cpus_to_monitor(kernel_max_cpu.value() + 1, true);

  // Rule out the related CPUs for each one so we only end up with the CPUs
  // that are representative of the cluster.
  for (unsigned int i = 0; i <= kernel_max_cpu.value(); i++) {
    if (!cpus_to_monitor[i]) {
      continue;
    }

    std::string filename = GetRelatedCPUsPathString(CpuId(i));
    std::string line;
    if (!base::ReadFileToString(base::FilePath(filename), &line)) {
      continue;
    }
    // When reading the related_cpus file, we expected the format to be
    // something like "0 1 2 3" for CPU0-3 if they're all in one cluster.
    for (auto& str_piece :
         base::SplitString(line, " ", base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY)) {
      unsigned int cpu_id;
      if (base::StringToUint(str_piece, &cpu_id)) {
        if (cpu_id != i && CpuId(cpu_id) <= kernel_max_cpu) {
          cpus_to_monitor[cpu_id] = false;
        }
      }
    }
    result.emplace_back(i);
  }

  // If none of the files were readable, we assume CPU0 exists and fall back to
  // using that.
  if (result.size() == 0) {
    result.emplace_back(0);
  }
  return result;
}

CPUFreqMonitor::CpuId CPUFreqMonitor::Delegate::GetKernelMaxCPUId() const {
  std::string str;
  if (!base::ReadFileToString(
          base::FilePath("/sys/devices/system/cpu/kernel_max"), &str)) {
    // If we fail to read the kernel_max file, we just assume that CPU0 exists.
    return CpuId(0);
  }

  unsigned int kernel_max_cpu = 0;
  base::StringToUint(str, &kernel_max_cpu);
  return CpuId(kernel_max_cpu);
}

std::string CPUFreqMonitor::Delegate::GetScalingCurFreqPathString(
    CpuId cpu_id) const {
  return absl::StrFormat(
      "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cpu_id.value());
}

std::string CPUFreqMonitor::Delegate::GetRelatedCPUsPathString(
    CpuId cpu_id) const {
  return absl::StrFormat("/sys/devices/system/cpu/cpu%d/cpufreq/related_cpus",
                         cpu_id.value());
}

CPUFreqMonitor::CPUFreqMonitor()
    : CPUFreqMonitor(std::make_unique<CPUFreqMonitor::Delegate>()) {}

CPUFreqMonitor::CPUFreqMonitor(
    std::unique_ptr<CPUFreqMonitor::Delegate> delegate)
    : delegate_(std::move(delegate)) {
  std::vector<CpuId> cpu_ids = delegate_->GetCPUIds();

  for (CpuId id : cpu_ids) {
    std::string fstr = delegate_->GetScalingCurFreqPathString(id);
    int fd = open(fstr.c_str(), O_RDONLY);
    if (fd == -1) {
      continue;
    }

    file_descriptors_.emplace_back(id, base::ScopedFD(fd));
  }
}

CPUFreqMonitor::~CPUFreqMonitor() = default;

std::vector<CPUFreqMonitor::CoreFrequency> CPUFreqMonitor::GetCoreFrequencies()
    const {
  std::vector<CoreFrequency> result;
  constexpr size_t kNumBytesToReadForSampling = 32;
  for (const auto& [id, fd] : file_descriptors_) {
    unsigned int freq = 0;
    // If we have trouble reading data from the file for any reason we'll end up
    // reporting the frequency as nothing.
    lseek(fd.get(), 0L, SEEK_SET);
    char data[kNumBytesToReadForSampling];

    ssize_t bytes_read = read(fd.get(), data, kNumBytesToReadForSampling);
    if (bytes_read > 0) {
      std::string_view content(data, bytes_read);
      content = base::TrimWhitespaceASCII(content, base::TRIM_ALL);
      if (!base::StringToUint(content, &freq)) {
        freq = 0;
      }
    }

    result.emplace_back(id, freq);
  }

  return result;
}

}  // namespace system_cpu
