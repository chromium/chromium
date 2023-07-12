// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines data types used to retrieve coalition metrics from the OS.

#ifndef COMPONENTS_POWER_METRICS_RESOURCE_COALITION_INTERNAL_TYPES_MAC_H_
#define COMPONENTS_POWER_METRICS_RESOURCE_COALITION_INTERNAL_TYPES_MAC_H_

#include <stdint.h>

// Comes from osfmk/mach/coalition.h
#define COALITION_TYPE_RESOURCE (0)
#define COALITION_TYPE_JETSAM (1)
#define COALITION_TYPE_MAX (1)

#define COALITION_NUM_TYPES (COALITION_TYPE_MAX + 1)

#define COALITION_NUM_THREAD_QOS_TYPES 7

// Comes from osfmk/mach/thread_policy.h
#define THREAD_QOS_UNSPECIFIED 0
#define THREAD_QOS_DEFAULT THREAD_QOS_UNSPECIFIED /* Temporary rename */
#define THREAD_QOS_MAINTENANCE 1
#define THREAD_QOS_BACKGROUND 2
#define THREAD_QOS_UTILITY 3
#define THREAD_QOS_LEGACY 4 /* i.e. default workq threads */
#define THREAD_QOS_USER_INITIATED 5
#define THREAD_QOS_USER_INTERACTIVE 6

#define THREAD_QOS_LAST 7

static_assert(COALITION_NUM_THREAD_QOS_TYPES == THREAD_QOS_LAST,
              "Unexpected number of QoS levels.");

// Comes from bsd/sys/coalition.h
struct coalition_resource_usage {
  uint64_t tasks_started;
  uint64_t tasks_exited;
  uint64_t time_nonempty;
  uint64_t cpu_time;
  uint64_t interrupt_wakeups;
  uint64_t platform_idle_wakeups;
  uint64_t bytesread;
  uint64_t byteswritten;
  uint64_t gpu_time;
  uint64_t cpu_time_billed_to_me;
  uint64_t cpu_time_billed_to_others;
  uint64_t energy;
  uint64_t logical_immediate_writes;
  uint64_t logical_deferred_writes;
  uint64_t logical_invalidated_writes;
  uint64_t logical_metadata_writes;
  uint64_t logical_immediate_writes_to_external;
  uint64_t logical_deferred_writes_to_external;
  uint64_t logical_invalidated_writes_to_external;
  uint64_t logical_metadata_writes_to_external;
  uint64_t energy_billed_to_me;
  uint64_t energy_billed_to_others;
  uint64_t cpu_ptime;
  uint64_t cpu_time_eqos_len; /* Stores the number of thread QoS types */
  uint64_t cpu_time_eqos[COALITION_NUM_THREAD_QOS_TYPES];
  uint64_t cpu_instructions;
  uint64_t cpu_cycles;
  uint64_t fs_metadata_writes;
  uint64_t pm_writes;
};

struct proc_pidcoalitioninfo {
  uint64_t coalition_id[COALITION_NUM_TYPES];
  uint64_t reserved1;
  uint64_t reserved2;
  uint64_t reserved3;
};

// Comes from bsd/sys/proc_info.h
#define PROC_PIDCOALITIONINFO 20

#endif  // COMPONENTS_POWER_METRICS_RESOURCE_COALITION_INTERNAL_TYPES_MAC_H_
