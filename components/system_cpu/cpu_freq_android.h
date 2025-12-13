// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_CPU_CPU_FREQ_ANDROID_H_
#define COMPONENTS_SYSTEM_CPU_CPU_FREQ_ANDROID_H_

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/types/id_type.h"

namespace system_cpu {

class CPUFreqMonitor {
 public:
  using CpuId = base::IdType<class CpuIdTag,
                             unsigned int,
                             std::numeric_limits<unsigned int>::max()>;
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Returns a vector of the minimal set of CPU IDs that we need to monitor to
    // get CPU frequency information. For CPUs that operate cores in a cluster,
    // i.e. modern Qualcomm 8 cores, this is CPU0 and CPU4.
    virtual std::vector<CpuId> GetCPUIds() const;

    // Reads the kernel_max_cpu file to determine the max CPU ID, i.e. 7 on an
    // 8-core CPU.
    virtual CpuId GetKernelMaxCPUId() const;

    // Gets the path to CPU frequency related files for a particular CPU ID.
    virtual std::string GetScalingCurFreqPathString(CpuId cpu_id) const;
    virtual std::string GetRelatedCPUsPathString(CpuId cpu_id) const;
  };

  CPUFreqMonitor();
  explicit CPUFreqMonitor(std::unique_ptr<CPUFreqMonitor::Delegate> delegate);
  ~CPUFreqMonitor();

  struct CoreFrequency {
    CpuId core_id;
    unsigned int freq;
  };

  std::vector<CoreFrequency> GetCoreFrequencies() const;

 private:
  std::unique_ptr<Delegate> delegate_;
  std::vector<std::pair<CpuId, base::ScopedFD>> file_descriptors_;
};

}  // namespace system_cpu

#endif  // COMPONENTS_SYSTEM_CPU_CPU_FREQ_ANDROID_H_
