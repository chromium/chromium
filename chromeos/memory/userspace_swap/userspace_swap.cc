// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/userspace_swap/userspace_swap.h"

#include <atomic>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chromeos/memory/userspace_swap/region.h"
#include "chromeos/memory/userspace_swap/swap_storage.h"
#include "chromeos/memory/userspace_swap/userfaultfd.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"

namespace chromeos {
namespace memory {
namespace userspace_swap {

namespace {

using memory_instrumentation::mojom::VmRegion;

// NOTE: Descriptions for these feature params can be found in the userspace
// swap header file for the UserspaceSwapConfig struct.
const base::Feature kUserspaceSwap{"UserspaceSwapEnabled",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<int> kUserspaceSwapPagesPerRegion = {
    &kUserspaceSwap, "UserspaceSwapPagesPerRegion", 16};
const base::FeatureParam<int> kUserspaceSwapVMARegionMinSizeKB = {
    &kUserspaceSwap, "UserspaceSwapVMARegionMinSizeKB", 1024};
const base::FeatureParam<int> kUserspaceSwapVMARegionMaxSizeKB = {
    &kUserspaceSwap, "UserspaceSwapVMARegionMaxSizeKB",
    1024 * 256 /* 256 MB */};
const base::FeatureParam<bool> kUserspaceSwapCompressedSwapFile = {
    &kUserspaceSwap, "UserspaceSwapCompressedSwapFile", true};
const base::FeatureParam<int> kUserspaceSwapMinSwapDeviceSpaceAvailMB = {
    &kUserspaceSwap, "UserspaceSwapMinSwapDeviceSpaceAvailMB", 128};
const base::FeatureParam<int> kUserspaceSwapMaximumSwapDiskSpaceMB = {
    &kUserspaceSwap, "UserspaceSwapMaximumSwapSpaceMB", 1024};
const base::FeatureParam<int> kUserspaceSwapRendererMaximumSwapDiskSpaceMB = {
    &kUserspaceSwap, "UserspaceSwapRendererMaximumSwapSpaceMB", 128};
const base::FeatureParam<int> kUserspaceSwapRendererRegionLimitPerSwap = {
    &kUserspaceSwap, "UserspaceSwapRendererRegionLimitPerSwap", 100};
const base::FeatureParam<int> kUserspaceSwapBlockedRefaultTimeSec = {
    &kUserspaceSwap, "UserspaceSwapBlockedRefaultTimeSec", 45};
const base::FeatureParam<int>
    kUserspaceSwapModeratePressureGraphWalkFrequencySec = {
        &kUserspaceSwap, "UserspaceSwapModeratePressureGraphWalkFrequencySec",
        60};
const base::FeatureParam<int> kUserspaceSwapProcessSwapFrequencySec = {
    &kUserspaceSwap, "UserspaceSwapProcessSwapFrequencySec", 120};
const base::FeatureParam<int> kUserspaceSwapInvisibleTimeBeforeSwapSec = {
    &kUserspaceSwap, "UserspaceSwapInvisibleTimeBeforeSwapSec", 60};
const base::FeatureParam<bool> kUserspaceDoSwapModeratePressure = {
    &kUserspaceSwap, "UserspaceSwapDoSwapOnModeratePressure", true};
const base::FeatureParam<bool> kUserspaceDoSwapOnFreeze = {
    &kUserspaceSwap, "UserspaceSwapDoSwapOnFreeze", true};
const base::FeatureParam<bool> kUserspaceSwapShuffleMapsOrder = {
    &kUserspaceSwap, "UserspaceSwapSuffleMapsOrder", true};

// g_global_disk_usage is the sum of all |written_to_disk| values from each
// renderer. We keep track of this number because we need to enforce the global
// total swap limit. This value is safe to be fetched from any sequence.
std::atomic<uint64_t> g_global_disk_usage_bytes = ATOMIC_VAR_INIT(0);

// This is the sum of all |reclaimed_bytes| values from each renderer. This
// value is safe to be fetched from any sequence.
std::atomic<uint64_t> g_global_reclaimed_bytes = ATOMIC_VAR_INIT(0);

class RendererSwapDataImpl : public RendererSwapData {
 public:
  RendererSwapDataImpl(
      int render_process_host_id,
      std::unique_ptr<chromeos::memory::userspace_swap::UserfaultFD>&& uffd,
      std::unique_ptr<chromeos::memory::userspace_swap::SwapFile>&& swap_file)
      : render_process_host_id_(render_process_host_id),
        uffd_(std::move(uffd)),
        swap_file_(std::move(swap_file)) {}

  ~RendererSwapDataImpl() override;

  // RendererSwapData impl:
  int render_process_host_id() const override;
  bool SwapAllowed() const override;
  void DisallowSwap() override;
  uint64_t SwapDiskspaceWrittenBytes() const override;
  uint64_t SwapDiskspaceUsedBytes() const override;
  uint64_t ReclaimedBytes() const override;

  // Account/UnaccountSwapSpace update all counters both for this renderer and
  // globally to reflect the swapped space.
  void AccountSwapSpace(int64_t reclaimed, int64_t swap_size);
  void UnaccountSwapSpace(int64_t reclaimed, int64_t swap_size);

 private:
  const int render_process_host_id_ = 0;
  bool swap_allowed_ = true;

  uint64_t on_disk_bytes_ = 0;
  uint64_t reclaimed_bytes_ = 0;

  std::unique_ptr<chromeos::memory::userspace_swap::UserfaultFD> uffd_;
  std::unique_ptr<chromeos::memory::userspace_swap::SwapFile> swap_file_;
};

int RendererSwapDataImpl::render_process_host_id() const {
  return render_process_host_id_;
}

void RendererSwapDataImpl::DisallowSwap() {
  swap_allowed_ = false;
}

bool RendererSwapDataImpl::SwapAllowed() const {
  return swap_allowed_;
}

uint64_t RendererSwapDataImpl::SwapDiskspaceWrittenBytes() const {
  return on_disk_bytes_;
}

uint64_t RendererSwapDataImpl::SwapDiskspaceUsedBytes() const {
  // Because punching a hole may not free the block if the region compressed
  // down to a partial size, the block can only be freed when all of it has been
  // punched. So we will take the larger of what we believe we've written to
  // disk and what the swap file reports as being in use.
  uint64_t swap_file_reported_disk_size_bytes =
      swap_file_->GetUsageKB() << 10;  // Convert to bytes from KB.
  uint64_t swap_file_disk_space_used_bytes =
      std::max(swap_file_reported_disk_size_bytes, on_disk_bytes_);

  return swap_file_disk_space_used_bytes;
}

uint64_t RendererSwapDataImpl::ReclaimedBytes() const {
  return reclaimed_bytes_;
}

void RendererSwapDataImpl::AccountSwapSpace(int64_t reclaimed,
                                            int64_t swap_size) {
  on_disk_bytes_ += swap_size;
  g_global_disk_usage_bytes += swap_size;
  reclaimed_bytes_ += reclaimed;
  g_global_reclaimed_bytes += reclaimed;
}

void RendererSwapDataImpl::UnaccountSwapSpace(int64_t reclaimed,
                                              int64_t swap_size) {
  AccountSwapSpace(-reclaimed, -swap_size);
}

RendererSwapDataImpl::~RendererSwapDataImpl() {}

}  // namespace

UserspaceSwapConfig::UserspaceSwapConfig() = default;
UserspaceSwapConfig::UserspaceSwapConfig(const UserspaceSwapConfig& other) =
    default;

// Static
CHROMEOS_EXPORT const UserspaceSwapConfig& UserspaceSwapConfig::Get() {
  static UserspaceSwapConfig config = []() -> UserspaceSwapConfig {
    UserspaceSwapConfig config = {};

    // Populate the config object.
    config.enabled = base::FeatureList::IsEnabled(kUserspaceSwap);
    config.number_of_pages_per_region = kUserspaceSwapPagesPerRegion.Get();
    config.vma_region_minimum_size_bytes =
        kUserspaceSwapVMARegionMinSizeKB.Get() << 10;  // Convert KB to bytes.
    config.vma_region_maximum_size_bytes =
        kUserspaceSwapVMARegionMaxSizeKB.Get() << 10;  // Convert KB to bytes.
    config.use_compressed_swap_file = kUserspaceSwapCompressedSwapFile.Get();
    config.minimum_swap_disk_space_available =
        kUserspaceSwapMinSwapDeviceSpaceAvailMB.Get()
        << 20;  // Convert MB to bytes.
    config.maximum_swap_disk_space_bytes =
        kUserspaceSwapMaximumSwapDiskSpaceMB.Get()
        << 20;  // Convert MB to bytes.
    config.renderer_maximum_disk_swap_file_size_bytes =
        kUserspaceSwapRendererMaximumSwapDiskSpaceMB.Get()
        << 20;  // Convert MB to bytes.
    config.renderer_region_limit_per_swap =
        kUserspaceSwapRendererRegionLimitPerSwap.Get();

    config.blocked_refault_time =
        base::TimeDelta::FromSeconds(kUserspaceSwapBlockedRefaultTimeSec.Get());
    config.graph_walk_frequency = base::TimeDelta::FromSeconds(
        kUserspaceSwapModeratePressureGraphWalkFrequencySec.Get());
    config.process_swap_frequency = base::TimeDelta::FromSeconds(
        kUserspaceSwapProcessSwapFrequencySec.Get());
    config.invisible_time_before_swap = base::TimeDelta::FromSeconds(
        kUserspaceSwapInvisibleTimeBeforeSwapSec.Get());

    config.swap_on_moderate_pressure = kUserspaceDoSwapModeratePressure.Get();
    config.swap_on_freeze = kUserspaceDoSwapOnFreeze.Get();
    config.shuffle_maps_on_swap = kUserspaceSwapShuffleMapsOrder.Get();

    return config;
  }();
  return config;
}

// An operator<< to allow us to print the values of a UserspaceSwapConfig to a
// stream.
std::ostream& operator<<(std::ostream& out, const UserspaceSwapConfig& c) {
  out << "UserspaceSwapConfig enabled: " << c.enabled << "\n";
  if (c.enabled) {
    out << "number_of_pages_per_region: " << c.number_of_pages_per_region
        << "\n";
    out << "vma_region_minimum_size_bytes: " << c.vma_region_minimum_size_bytes
        << "\n";
    out << "vma_region_maximum_size_bytes: " << c.vma_region_maximum_size_bytes
        << "\n";
    out << "use_compressed_swap: " << c.use_compressed_swap_file << "\n";
    out << "minimum_swap_disk_space_available: "
        << c.minimum_swap_disk_space_available << "\n";
    out << "maximum_swap_disk_space_bytes: " << c.maximum_swap_disk_space_bytes
        << "\n";
    out << "renderer_maximum_disk_swap_file_size_bytes: "
        << c.renderer_maximum_disk_swap_file_size_bytes << "\n";
    out << "renderer_region_limit_per_swap: "
        << c.renderer_region_limit_per_swap << "\n";
    out << "blocked_refault_time: " << c.blocked_refault_time << "\n";
    out << "graph_walk_frequency: " << c.graph_walk_frequency << "\n";
    out << "process_swap_frequency: " << c.process_swap_frequency << "\n";
    out << "invisible_time_before_swap: " << c.invisible_time_before_swap
        << "\n";
    out << "swap_on_freeze: " << c.swap_on_freeze << "\n";
    out << "swap_on_moderate_pressure: " << c.swap_on_moderate_pressure << "\n";
    out << "shuffle_maps_on_swap: " << c.shuffle_maps_on_swap << "\n";
  }
  return out;
}

// KernelSupportsUserspaceSwap will test for all features necessary to enbable
// userspace swap.
CHROMEOS_EXPORT bool KernelSupportsUserspaceSwap() {
  static bool userfault_fd_supported = chromeos::memory::userspace_swap::
      UserfaultFD::KernelSupportsUserfaultFD();

  // We also need to make sure the kernel supports the mremap operation with
  // MREMAP_DONTUNMAP.
  static bool mremap_dontunmap_supported = []() -> bool {
    const size_t allocation_size = base::GetPageSize();
    void* source_mapping = mmap(NULL, allocation_size, PROT_NONE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (source_mapping == MAP_FAILED) {
      return false;
    }

    // This simple remap should only fail if MREMAP_DONTUNMAP isn't
    // supported.
    void* dest_mapping =
        mremap(source_mapping, allocation_size, allocation_size,
               MREMAP_DONTUNMAP | MREMAP_MAYMOVE, 0);
    if (dest_mapping == MAP_FAILED) {
      munmap(source_mapping, allocation_size);
      return false;
    }

    munmap(dest_mapping, allocation_size);
    munmap(source_mapping, allocation_size);
    return true;
  }();

  return userfault_fd_supported && mremap_dontunmap_supported;
}

RendererSwapData::RendererSwapData() {}
RendererSwapData::~RendererSwapData() {}

// static
CHROMEOS_EXPORT std::unique_ptr<RendererSwapData> RendererSwapData::Create(
    int render_process_host_id,
    std::unique_ptr<chromeos::memory::userspace_swap::UserfaultFD> uffd,
    std::unique_ptr<chromeos::memory::userspace_swap::SwapFile> swap_file) {
  return std::make_unique<RendererSwapDataImpl>(
      render_process_host_id, std::move(uffd), std::move(swap_file));
}

CHROMEOS_EXPORT bool UserspaceSwapSupportedAndEnabled() {
  static bool enabled = UserspaceSwapConfig::Get().enabled;
  static bool supported = KernelSupportsUserspaceSwap();
  return supported && enabled;
}

CHROMEOS_EXPORT bool SwapRegions(RendererSwapData* data, int num_regions) {
  // TODO(bgeffon): We need to now land all of the process specific swap code.
  RendererSwapDataImpl* impl = reinterpret_cast<RendererSwapDataImpl*>(data);
  VLOG(1) << "SwapRegions for rphid " << impl->render_process_host_id()
          << " at most " << num_regions << " regions";
  return true;
}

CHROMEOS_EXPORT bool IsVMASwapEligible(
    const memory_instrumentation::mojom::VmRegionPtr& vma) {
  // We only conisder VMAs which are Private Anonymous
  // Readable/Writable that aren't locked and a certain size.
  uint32_t target_perms =
      VmRegion::kProtectionFlagsRead | VmRegion::kProtectionFlagsWrite;
  if (vma->protection_flags != target_perms)
    return false;

  if (!vma->mapped_file.empty())
    return false;

  if (vma->byte_locked > 0)
    return false;

  // It must be within the VMA size bounds configured.
  const auto& config = UserspaceSwapConfig::Get();
  if (vma->size_in_bytes < config.vma_region_minimum_size_bytes ||
      vma->size_in_bytes > config.vma_region_maximum_size_bytes)
    return false;

  return true;
}

CHROMEOS_EXPORT bool GetAllSwapEligibleVMAs(base::PlatformThreadId pid,
                                            std::vector<Region>* regions) {
  DCHECK(regions);
  regions->clear();

  const auto& config = UserspaceSwapConfig::Get();

  std::vector<memory_instrumentation::mojom::VmRegionPtr> vmas =
      memory_instrumentation::OSMetrics::GetProcessMemoryMaps(pid);

  if (vmas.empty()) {
    return false;
  }

  // Only consider VMAs which match our criteria.
  for (const auto& v : vmas) {
    if (IsVMASwapEligible(v)) {
      regions->push_back(Region(static_cast<uintptr_t>(v->start_address),
                                static_cast<uintptr_t>(v->size_in_bytes)));
    }
  }

  // We can shuffle the VMA maps (if configured) so we don't always start from
  // the same VMA on subsequent swaps.
  if (config.shuffle_maps_on_swap) {
    base::RandomShuffle(regions->begin(), regions->end());
  }

  return true;
}

CHROMEOS_EXPORT uint64_t GetGlobalMemoryReclaimed() {
  return g_global_reclaimed_bytes.load();
}

CHROMEOS_EXPORT uint64_t GetGlobalSwapDiskspaceUsed() {
  return g_global_disk_usage_bytes.load();
}

}  // namespace userspace_swap
}  // namespace memory
}  // namespace chromeos
