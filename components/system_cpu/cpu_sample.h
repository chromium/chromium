// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYSTEM_CPU_CPU_SAMPLE_H_
#define COMPONENTS_SYSTEM_CPU_CPU_SAMPLE_H_

namespace system_cpu {

// Represents availability of compute resources measured over a period of time.
struct CpuSample {
  // Average utilization of all CPU cores.
  //
  // Values use a scale from 0.0 (no utilization) to 1.0 (100% utilization).
  double cpu_utilization;
};

}  // namespace system_cpu

#endif  // COMPONENTS_SYSTEM_CPU_CPU_SAMPLE_H_
