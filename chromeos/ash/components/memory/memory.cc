// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/memory.h"

#include <link.h>
#include <sys/mman.h>
#include "base/bit_cast.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/page_size.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/memory/swap_configuration.h"

namespace ash {

namespace memory {

namespace internal {

COMPONENT_EXPORT(ASH_MEMORY)
bool ParseZramMmStat(const std::string& input, ZramMmStat* zram_mm_stat) {
  std::vector<std::string> zram_mm_stat_list = base::SplitString(
      input, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Return false if the list size is less than number of items in ZramMmStat
  // From first version of Zram mm_stat in v4.4, there are seven fields inside.
  if (zram_mm_stat_list.size() < 7) {
    LOG(ERROR) << "Malformed zram mm_stat input";
    return false;
  }
  // In zram_drv.h we define max_used_pages as atomic_long_t which could
  // be negative, but negative value does not make sense for the
  // variable. return false if negative max_used_pages.
  int64_t tmp_mem_used_max = 0;
  if (!base::StringToInt64(zram_mm_stat_list[4], &tmp_mem_used_max) ||
      tmp_mem_used_max < 0) {
    LOG(ERROR) << "Bad value for zram max_used_pages";
    return false;
  }
  zram_mm_stat->mem_used_max = static_cast<uint64_t>(tmp_mem_used_max);

  bool status =
      base::StringToUint64(zram_mm_stat_list[0],
                           &zram_mm_stat->orig_data_size) &&
      base::StringToUint64(zram_mm_stat_list[1],
                           &zram_mm_stat->compr_data_size) &&
      base::StringToUint64(zram_mm_stat_list[2],
                           &zram_mm_stat->mem_used_total) &&
      base::StringToUint(zram_mm_stat_list[3], &zram_mm_stat->mem_limit) &&
      base::StringToUint64(zram_mm_stat_list[5], &zram_mm_stat->same_pages) &&
      base::StringToUint(zram_mm_stat_list[6], &zram_mm_stat->pages_compacted);

  constexpr static size_t kHugeIdx = 7;
  constexpr static size_t kHugeSinceIdx = 8;

  if (zram_mm_stat_list.size() > kHugeIdx) {
    uint64_t value = 0;
    status &= base::StringToUint64(zram_mm_stat_list[kHugeIdx], &value);
    if (status) {
      zram_mm_stat->huge_pages = value;
    }
  }

  if (zram_mm_stat_list.size() > kHugeSinceIdx) {
    uint64_t value = 0;
    status &= base::StringToUint64(zram_mm_stat_list[kHugeSinceIdx], &value);
    if (status) {
      zram_mm_stat->huge_pages_since = value;
    }
  }

  return status;
}

COMPONENT_EXPORT(ASH_MEMORY)
bool ParseZramBdStat(const std::string& input, ZramBdStat* zram_bd_stat) {
  std::vector<std::string> zram_bd_stat_list = base::SplitString(
      input, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Return false if the list size is less than number of items in ZramBdStat
  if (zram_bd_stat_list.size() < 3) {
    LOG(ERROR) << "Malformed zram bd_stat input";
    return false;
  }

  return base::StringToUint64(zram_bd_stat_list[0], &zram_bd_stat->bd_count) &&
         base::StringToUint64(zram_bd_stat_list[1], &zram_bd_stat->bd_reads) &&
         base::StringToUint64(zram_bd_stat_list[2], &zram_bd_stat->bd_writes);
}

COMPONENT_EXPORT(ASH_MEMORY)
bool ParseZramIoStat(const std::string& input, ZramIoStat* zram_io_stat) {
  std::vector<std::string> zram_io_stat_list = base::SplitString(
      input, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Return false if the list size is less than number of items in ZramIoStat
  if (zram_io_stat_list.size() < 4) {
    LOG(ERROR) << "Malformed zram io_stat input";
    return false;
  }

  return base::StringToUint64(zram_io_stat_list[0],
                              &zram_io_stat->failed_reads) &&
         base::StringToUint64(zram_io_stat_list[1],
                              &zram_io_stat->failed_writes) &&
         base::StringToUint64(zram_io_stat_list[2],
                              &zram_io_stat->invalid_io) &&
         base::StringToUint64(zram_io_stat_list[3], &zram_io_stat->notify_free);
}

}  // namespace internal

bool GetZramMmStatsForDevice(ZramMmStat* zram_mm_stat, uint8_t dev_id) {
  std::string buf;
  base::FilePath mm_stat_path("/sys/block/zram" + base::NumberToString(dev_id) +
                              "/mm_stat");
  if (!base::ReadFileToStringNonBlocking(mm_stat_path, &buf)) {
    return false;
  }

  return internal::ParseZramMmStat(buf, zram_mm_stat);
}

bool GetZramBdStatsForDevice(ZramBdStat* zram_bd_stat, uint8_t dev_id) {
  std::string buf;
  base::FilePath bd_stat_path("/sys/block/zram" + base::NumberToString(dev_id) +
                              "/bd_stat");
  if (!base::ReadFileToStringNonBlocking(bd_stat_path, &buf)) {
    return false;
  }

  return internal::ParseZramBdStat(buf, zram_bd_stat);
}

bool GetZramIoStatsForDevice(ZramIoStat* zram_io_stat, uint8_t dev_id) {
  std::string buf;
  base::FilePath io_stat_path("/sys/block/zram" + base::NumberToString(dev_id) +
                              "/io_stat");
  if (!base::ReadFileToStringNonBlocking(io_stat_path, &buf)) {
    return false;
  }

  return internal::ParseZramIoStat(buf, zram_io_stat);
}

bool GetZramMmStats(ZramMmStat* zram_mm_stat) {
  // Get the first and default zram device mm stats.
  return GetZramMmStatsForDevice(zram_mm_stat, 0);
}

bool GetZramBdStats(ZramBdStat* zram_bd_stat) {
  // Get the first and default zram device bd stats.
  return GetZramBdStatsForDevice(zram_bd_stat, 0);
}

bool GetZramIoStats(ZramIoStat* zram_io_stat) {
  // Get the first and default zram device io stats.
  return GetZramIoStatsForDevice(zram_io_stat, 0);
}

ZramMetrics::ZramMetrics() = default;
ZramMetrics::~ZramMetrics() = default;

bool ZramMetrics::CollectEvents() {
  ZramMmStat zram_mm_stat;

  if (!GetZramMmStats(&zram_mm_stat)) {
    return false;
  }

  const int kTotalPagesSwapped =
      zram_mm_stat.orig_data_size / base::GetPageSize();

  orig_data_size_mb_ = zram_mm_stat.orig_data_size / kMB;
  UMA_HISTOGRAM_MEMORY_LARGE_MB("ChromeOS.Zram.OrigDataSizeMB",
                                orig_data_size_mb_);
  UMA_HISTOGRAM_MEMORY_LARGE_MB("ChromeOS.Zram.ComprDataSizeMB",
                                zram_mm_stat.compr_data_size / kMB);

  UMA_HISTOGRAM_PERCENTAGE(
      "ChromeOS.Zram.CompressedSizePct",
      zram_mm_stat.orig_data_size
          ? (zram_mm_stat.compr_data_size * 100.0 / zram_mm_stat.orig_data_size)
          : 0);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("ChromeOS.Zram.MemUsedTotalMB",
                                zram_mm_stat.mem_used_total / kMB);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("ChromeOS.Zram.MemLimitMB",
                                zram_mm_stat.mem_limit / kMB);

  UMA_HISTOGRAM_MEMORY_LARGE_MB("ChromeOS.Zram.MemUsedMaxMB",
                                zram_mm_stat.mem_used_max / kMB);

  UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeOS.Zram.SamePages",
                              zram_mm_stat.same_pages, 1, kMaxNumPages, 50);

  UMA_HISTOGRAM_PERCENTAGE(
      "ChromeOS.Zram.SamePagesPct",
      kTotalPagesSwapped ? zram_mm_stat.same_pages * 100.0 / kTotalPagesSwapped
                         : 0);

  UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeOS.Zram.PagesCompacted",
                              zram_mm_stat.pages_compacted, 1, kMaxNumPages,
                              50);

  if (zram_mm_stat.huge_pages) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeOS.Zram.HugePages",
                                *zram_mm_stat.huge_pages, 1, kMaxNumPages, 50);

    UMA_HISTOGRAM_PERCENTAGE("ChromeOS.Zram.HugePagesPct",
                             kTotalPagesSwapped ? *zram_mm_stat.huge_pages *
                                                      100.0 / kTotalPagesSwapped
                                                : 0);

    if (zram_mm_stat.huge_pages_since) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeOS.Zram.HugePagesSince",
                                  *zram_mm_stat.huge_pages_since, 1,
                                  kMaxNumPages, 50);

      if (has_old_huge_pages_) {
        int64_t stored = *zram_mm_stat.huge_pages_since - old_huge_pages_since_;
        // The delta in 'stored' minus the growth in state is the number of
        // pages removed.
        int64_t removed = stored - (*zram_mm_stat.huge_pages - old_huge_pages_);
        if (stored >= 0 && removed >= 0) {
          UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeOS.Zram.HugePagesStored", stored,
                                      1, kMaxNumPages, 50);
          UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeOS.Zram.HugePagesRemoved", removed,
                                      1, kMaxNumPages, 50);
        }
      }
      // Save for next time.
      has_old_huge_pages_ = true;
      old_huge_pages_ = *zram_mm_stat.huge_pages;
      old_huge_pages_since_ = *zram_mm_stat.huge_pages_since;
    }
  }

  ZramBdStat zram_bd_stat;

  if (!GetZramBdStats(&zram_bd_stat)) {
    return false;
  }

  UMA_HISTOGRAM_COUNTS_1M("ChromeOS.Zram.BdCount", zram_bd_stat.bd_count);

  UMA_HISTOGRAM_COUNTS_1M("ChromeOS.Zram.BdReads", zram_bd_stat.bd_reads);

  UMA_HISTOGRAM_COUNTS_1M("ChromeOS.Zram.BdWrites", zram_bd_stat.bd_writes);

  ZramIoStat zram_io_stat;

  if (!GetZramIoStats(&zram_io_stat)) {
    return false;
  }

  UMA_HISTOGRAM_COUNTS_1000("ChromeOS.Zram.FailedReads",
                            zram_io_stat.failed_reads);

  UMA_HISTOGRAM_COUNTS_1000("ChromeOS.Zram.FailedWrites",
                            zram_io_stat.failed_writes);

  UMA_HISTOGRAM_COUNTS_1000("ChromeOS.Zram.InvalidIo", zram_io_stat.invalid_io);

  UMA_HISTOGRAM_COUNTS_1000("ChromeOS.Zram.NotifyFree",
                            zram_io_stat.notify_free);
  return true;
}

}  // namespace memory
}  // namespace ash
