// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_MEMORY_H_
#define CHROMEOS_MEMORY_MEMORY_H_

#include <cstddef>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// MlockMaping will attempt to mlock a mapping using the newer mlock2 syscall
// if available using the MLOCK_ONFAULT option. This will allow pages to be
// locked as they are faulted in. If the running kernel does not support
// mlock2, it was added in kernel 4.4, it will fall back to mlock where it
// will lock all pages immediately by faulting them in.
CHROMEOS_EXPORT bool MlockMapping(void* addr, size_t length);

// A feature which controls the locking the main program text.
extern const base::Feature kCrOSLockMainProgramText;

// The maximum number of bytes that the browser will attempt to lock, -1 will
// disable the max size and is the default option.
extern const base::FeatureParam<int> kCrOSLockMainProgramTextMaxSize;

// Lock main program text segments fully.
CHROMEOS_EXPORT void LockMainProgramText();

// It should be called when some memory configuration is changed.
CHROMEOS_EXPORT void UpdateMemoryParameters();

namespace memory {

struct ZramMmStat {
  // Uncompressed size of data stored in this disk. This excludes
  // same-element-filled pages (same_pages) since no memory is allocated for
  // them. Unit: bytes
  uint64_t orig_data_size;
  // Compressed size of data stored in this disk.
  uint64_t compr_data_size;
  // The amount of memory allocated for this disk. This includes allocator
  // fragmentation and metadata overhead, allocated for this disk. So, allocator
  // space efficiency can be calculated using compr_data_size and this
  // statistic. Unit: bytes
  uint64_t mem_used_total;
  // The maximum amount of memory ZRAM can use to store The compressed data.
  uint32_t mem_limit;
  // The maximum amount of memory zram have consumed to store the data.
  uint64_t mem_used_max;
  // The number of same element filled pages written to this disk. No memory is
  // allocated for such pages.
  uint64_t same_pages;
  // The number of pages freed during compaction.
  uint32_t pages_compacted;
  // The number of incompressible pages.
  // Start supporting from v4.19.
  uint64_t huge_pages;
  // The number of huge pages since zram set up.
  // Start supporting from v5.15.
  uint64_t huge_pages_since;
};

struct ZramBdStat {
  // Size of data written in backing device. Unit: 4K bytes
  uint64_t bd_count;
  // The number of reads from backing device. Unit: 4K bytes
  uint64_t bd_reads;
  // The number of writes to backing device. Unit: 4K bytes
  uint64_t bd_writes;
};

struct ZramIoStat {
  // The number of failed reads.
  uint64_t failed_reads;
  // The number of failed writes.
  uint64_t failed_writes;
  // The number of non-page-size-aligned I/O requests
  uint64_t invalid_io;
  // Depending on device usage scenario it may account a) the number of pages
  // freed because of swap slot free notifications or b) the number of pages
  // freed because of REQ_OP_DISCARD requests sent by bio. The former ones are
  // sent to a swap block device when a swap slot is freed, which implies that
  // this disk is being used as a swap disk. The latter ones are sent by
  // filesystem mounted with discard option, whenever some data blocks are
  // getting discarded.
  uint64_t notify_free;
};

namespace internal {
CHROMEOS_EXPORT bool ParseZramMmStat(const std::string& input,
                                     ZramMmStat* zram_mm_stat);

CHROMEOS_EXPORT bool ParseZramBdStat(const std::string& input,
                                     ZramBdStat* zram_bd_stat);

CHROMEOS_EXPORT bool ParseZramIoStat(const std::string& input,
                                     ZramIoStat* zram_io_stat);

}  // namespace internal

bool GetZramMmStatsForDevice(ZramMmStat* zram_mm_stat,
                             uint8_t dev_id);  // id for zram block

bool GetZramBdStatsForDevice(ZramBdStat* zram_bd_stat,
                             uint8_t dev_id);  // id for zram block

bool GetZramIoStatsForDevice(ZramIoStat* zram_io_stat,
                             uint8_t dev_id);  // id for zram block

// The function is used to read Zram block id 0 mmstat.
bool GetZramMmStats(ZramMmStat* zram_mm_stat);

// The function is used to read Zram block id 0 bdstat.
bool GetZramBdStats(ZramBdStat* zram_bd_stat);

// The function is used to read Zram block id 0 iostat.
bool GetZramIoStats(ZramIoStat* zram_io_stat);

}  // namespace memory
}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_MEMORY_H_
