// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/cpu_metrics_provider.h"

#include "base/cpu.h"
#include "base/system/sys_info.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

CPUMetricsProvider::CPUMetricsProvider() = default;

CPUMetricsProvider::~CPUMetricsProvider() = default;

void CPUMetricsProvider::ProvideSystemProfileMetrics(
    SystemProfileProto* system_profile) {
  SystemProfileProto::Hardware::CPU* cpu =
      system_profile->mutable_hardware()->mutable_cpu();
  // All the CPU information is generated in the constructor.
  base::CPU cpu_info;
  cpu->set_vendor_name(cpu_info.vendor_name());
  cpu->set_signature(cpu_info.signature());
  cpu->set_num_cores(base::SysInfo::NumberOfProcessors());
  cpu->set_is_hypervisor(cpu_info.is_running_in_vm());
}

}  // namespace metrics
