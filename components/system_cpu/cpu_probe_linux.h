// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_CPU_CPU_PROBE_LINUX_H_
#define COMPONENTS_SYSTEM_CPU_CPU_PROBE_LINUX_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "components/system_cpu/cpu_probe.h"

namespace system_cpu {

class CpuProbeLinux : public CpuProbe {
 public:
  // Factory method for production instances.
  static std::unique_ptr<CpuProbeLinux> Create();

  ~CpuProbeLinux() override;

  CpuProbeLinux(const CpuProbeLinux&) = delete;
  CpuProbeLinux& operator=(const CpuProbeLinux&) = delete;

 protected:
  explicit CpuProbeLinux(base::FilePath);

  // CpuProbe implementation.
  void Update(SampleCallback callback) override;
  base::WeakPtr<CpuProbe> GetWeakPtr() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CpuProbeLinuxTest, ProductionDataNoCrash);
  FRIEND_TEST_ALL_PREFIXES(CpuProbeLinuxTest, OneCoreFullInfo);
  FRIEND_TEST_ALL_PREFIXES(CpuProbeLinuxTest, TwoCoresFullInfo);
  FRIEND_TEST_ALL_PREFIXES(CpuProbeLinuxTest, TwoCoresSecondCoreMissingStat);

  class BlockingTaskRunnerHelper;

  base::SequenceBound<BlockingTaskRunnerHelper> helper_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<CpuProbeLinux> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace system_cpu

#endif  // COMPONENTS_SYSTEM_CPU_CPU_PROBE_LINUX_H_
