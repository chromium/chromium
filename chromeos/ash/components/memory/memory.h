// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_MEMORY_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_MEMORY_H_

#include <cstddef>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace memory {

class COMPONENT_EXPORT(ASH_MEMORY) ZramMetrics
    : public base::RefCountedThreadSafe<ZramMetrics> {
 public:
  // Max number of pages should be the max system memory divided by the smallest
  // possible page size, or: 16GB / 4096
  static constexpr uint64_t kMaxNumPages =
      (static_cast<uint64_t>(16) << 30) / (4096);
  static constexpr int kMB = 1 << 20;

  ZramMetrics();
  ZramMetrics(const ZramMetrics&) = delete;
  ZramMetrics& operator=(const ZramMetrics&) = delete;

  // Must be called on a background sequence. Updates the cached instance of
  // orig_data_size_mb_. Returns false if there's an error.
  bool CollectEvents();

  uint32_t orig_data_size_mb() const { return orig_data_size_mb_; }

 private:
  // Friend it so it can call our private destructor.
  friend class base::RefCountedThreadSafe<ZramMetrics>;

  ~ZramMetrics();

  // Last-time old-pages stats for delta computation
  // (only for kernel v5.15+), using |has_old_huge_pages_| to determine
  // whether |old_huge_pages_| and |old_huge_pages_since_| are valid.
  bool has_old_huge_pages_ = false;
  uint64_t old_huge_pages_ = 0;
  uint64_t old_huge_pages_since_ = 0;

  // A cached instance of OrigDataSizeMB. Only valid if CollectEvents returns
  // true.
  uint32_t orig_data_size_mb_ = 0;

  // The background task runner where the collection takes place.
  scoped_refptr<base::SequencedTaskRunner> runner_;
};

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
  absl::optional<uint64_t> huge_pages;
  // The number of huge pages since zram set up.
  // Start supporting from v5.15.
  absl::optional<uint64_t> huge_pages_since;
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

COMPONENT_EXPORT(ASH_MEMORY)
bool ParseZramMmStat(const std::string& input, ZramMmStat* zram_mm_stat);

COMPONENT_EXPORT(ASH_MEMORY)
bool ParseZramBdStat(const std::string& input, ZramBdStat* zram_bd_stat);

COMPONENT_EXPORT(ASH_MEMORY)
bool ParseZramIoStat(const std::string& input, ZramIoStat* zram_io_stat);

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
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_MEMORY_H_
