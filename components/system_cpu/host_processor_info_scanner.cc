// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/system_cpu/host_processor_info_scanner.h"

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <stdint.h>

#include "base/apple/scoped_mach_port.h"
#include "base/apple/scoped_mach_vm.h"
#include "base/mac/mac_util.h"
#include "base/system/sys_info.h"
#include "components/system_cpu/core_times.h"

namespace system_cpu {

HostProcessorInfoScanner::HostProcessorInfoScanner() {
  core_times_.reserve(base::SysInfo::NumberOfProcessors());
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

HostProcessorInfoScanner::~HostProcessorInfoScanner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool HostProcessorInfoScanner::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (base::mac::GetCPUType() == base::mac::CPUType::kTranslatedIntel) {
    // ARM-based macs are not supported because Rosetta's simulation is not
    // complete. We will skip obtaining CPU usage information. See
    // https://crbug.com/1138707#c42 for details.
    return false;
  }

  natural_t number_of_processors;
  base::apple::ScopedMachSendRight host(mach_host_self());
  mach_msg_type_number_t type;
  processor_cpu_load_info_data_t* cpu_infos;

  if (host_processor_info(host.get(), PROCESSOR_CPU_LOAD_INFO,
                          &number_of_processors,
                          reinterpret_cast<processor_info_array_t*>(&cpu_infos),
                          &type) != KERN_SUCCESS) {
    return false;
  }

  base::apple::ScopedMachVM vm_owner(
      reinterpret_cast<vm_address_t>(cpu_infos),
      mach_vm_round_page(number_of_processors *
                         sizeof(processor_cpu_load_info)));

  for (natural_t i = 0; i < number_of_processors; ++i) {
    CHECK_GE(core_times_.size(), i);

    if (core_times_.size() <= i) {
      core_times_.resize(i + 1);
      CoreTimes core_time;
      core_times_[i] = std::move(core_time);
    }

    core_times_[i].set_user(cpu_infos[i].cpu_ticks[CPU_STATE_USER]);
    core_times_[i].set_nice(cpu_infos[i].cpu_ticks[CPU_STATE_NICE]);
    core_times_[i].set_system(cpu_infos[i].cpu_ticks[CPU_STATE_SYSTEM]);
    core_times_[i].set_idle(cpu_infos[i].cpu_ticks[CPU_STATE_IDLE]);
  }

  return true;
}

}  // namespace system_cpu
