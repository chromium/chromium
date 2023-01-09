// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/zram_writeback_policy.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/memory/memory.h"
#include "chromeos/ash/components/memory/swap_configuration.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::memory {

namespace {

// We are not considering anything other than 4K pages at the moment.
constexpr uint64_t kPagesize = 4096;
constexpr int kMBtoBytesShift = 20;

class ZramWritebackPolicyImpl : public ZramWritebackPolicy {
 public:
  ZramWritebackPolicyImpl() { SetParams(ZramWritebackParams::Get()); }
  ~ZramWritebackPolicyImpl() override = default;

  base::TimeDelta GetWritebackTimerInterval() override {
    return params_.periodic_time;
  }

  void Initialize(uint64_t zram_disk_size_mb,
                  uint64_t writeback_size_mb) override {
    writeback_device_size_pages_ =
        (writeback_size_mb << kMBtoBytesShift) / kPagesize;
    zram_size_pages_ = (zram_disk_size_mb << kMBtoBytesShift) / kPagesize;
  }

  base::TimeDelta GetCurrentWritebackIdleTime() override {
    if (!CanWritebackIdle()) {
      return base::TimeDelta::Max();
    }

    if (!UpdateMemoryInfo()) {
      return base::TimeDelta::Max();
    }

    // Stay between idle_(min|max)_time.
    uint64_t min_sec = params_.idle_min_time.InSeconds();
    uint64_t max_sec = params_.idle_max_time.InSeconds();
    double mem_utilization =
        (1.0 -
         (static_cast<double>(memory_info_.available) / memory_info_.total));

    // Exponentially decay the writeback age vs. memory utilization. The reason
    // we choose exponential decay is because we want to do as little work as
    // possible when the system is under very low memory pressure. As pressure
    // increases we want to start aggressively shrinking our idle age to force
    // newer pages to be written back.
    constexpr double kLambda = 5;
    uint64_t age_sec =
        (max_sec - min_sec) * pow(M_E, -kLambda * mem_utilization) + min_sec;

    return base::Seconds(age_sec);
  }

  bool CanWritebackHugeIdle() override { return params_.writeback_huge_idle; }
  bool CanWritebackIdle() override { return params_.writeback_idle; }
  bool CanWritebackHuge() override { return params_.writeback_huge; }

  uint64_t GetAllowedWritebackLimit() override {
    // We haven't completed initialization.
    if (writeback_device_size_pages_ == 0) {
      LOG(WARNING) << "GetAllowedWritebackLimit called before Initialize";
      NOTREACHED();
      return 0;
    }

    // Basic sanity check on our configuration.
    if (!CanWritebackIdle() && !CanWritebackHuge() && !CanWritebackHugeIdle()) {
      return 0;
    }

    // Did we writeback too recently?
    if (last_writeback_ != base::Time::Min()) {
      const auto time_since_writeback = base::Time::Now() - last_writeback_;
      if (time_since_writeback < params_.backoff_time) {
        return 0;
      }
    }

    // We need to decide how many pages we will want to write back total, this
    // includes huge and idle if they are both enabled. The calculation is based
    // on zram utilization, writeback utilization, and memory pressure.
    uint64_t num_pages = 0;

    // Update metrics so we can make a decision.
    if (!UpdateZramMetrics()) {
      return 0;
    }

    // All calculations are performed in basis points, 100 bps = 1.00%. The
    // number of pages allowed to be written back follows a simple linear
    // relationship. The allowable range is [min_pages, max_pages], and the
    // writeback limit will be the (zram utilization) * the range, that is, the
    // more zram we're using the more we're going to allow to be written back.
    constexpr uint32_t kBps = 100 * 100;
    uint64_t pages_currently_written_back = zram_bd_stats_.bd_count;
    uint32_t zram_utilization_bps =
        ((zram_mm_stats_.orig_data_size / kPagesize) * kBps) / zram_size_pages_;
    num_pages = zram_utilization_bps * params_.max_pages / kBps;

    // And try to limit it to the approximate number of free backing device
    // pages (if it's less).
    uint64_t free_bd_pages =
        writeback_device_size_pages_ - pages_currently_written_back;
    num_pages = std::min(num_pages, free_bd_pages);

    // Finally enforce the limits, we won't even attempt writeback if we
    // cannot writeback at least the min, and we will cap to the max if it's
    // greater.
    num_pages = std::min(num_pages, params_.max_pages);
    if (num_pages < params_.min_pages) {
      // Configured to not writeback fewer than kCrOSWritebackMinPages.
      return 0;
    }

    last_writeback_ = base::Time::Now();
    return num_pages;
  }

 protected:
  virtual bool UpdateZramMetrics() {
    // Refresh our current metrics.
    if (!GetZramMmStats(&zram_mm_stats_)) {
      LOG(ERROR) << "Unable to get zram mm stats";
      return false;
    }

    if (!GetZramBdStats(&zram_bd_stats_)) {
      LOG(ERROR) << "Unable to get zram bd stats";
      return false;
    }

    return true;
  }

  virtual bool UpdateMemoryInfo() {
    return base::GetSystemMemoryInfo(&memory_info_);
  }

  virtual void SetParams(const ZramWritebackParams& params) {
    params_ = params;
  }

  ZramMmStat zram_mm_stats_;
  ZramBdStat zram_bd_stats_;
  base::SystemMemoryInfoKB memory_info_;

  ZramWritebackParams params_;

  uint64_t writeback_device_size_pages_ = 0;
  uint64_t zram_size_pages_ = 0;

  base::Time last_writeback_{base::Time::Min()};
};

}  // namespace

// static
std::unique_ptr<ZramWritebackPolicy> COMPONENT_EXPORT(ASH_MEMORY)
    ZramWritebackPolicy::Get() {
  return std::make_unique<ZramWritebackPolicyImpl>();
}

}  // namespace ash::memory
