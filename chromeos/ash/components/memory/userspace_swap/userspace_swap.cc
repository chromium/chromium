// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/memory/page_size.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chromeos/ash/components/memory/aligned_memory.h"
#include "chromeos/ash/components/memory/pagemap.h"
#include "chromeos/ash/components/memory/userspace_swap/region.h"
#include "chromeos/ash/components/memory/userspace_swap/swap_storage.h"
#include "chromeos/ash/components/memory/userspace_swap/userfaultfd.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.mojom-forward.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace ash {
namespace memory {
namespace userspace_swap {

namespace {

using memory_instrumentation::mojom::VmRegion;

// NOTE: Descriptions for these feature params can be found in the userspace
// swap header file for the UserspaceSwapConfig struct.
BASE_FEATURE(kUserspaceSwap,
             "UserspaceSwapEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kUserspaceSwapPagesPerRegion = {
    &kUserspaceSwap, "UserspaceSwapPagesPerRegion", 16};
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
std::atomic<uint64_t> g_global_disk_usage_bytes{0};

// This is the sum of all |reclaimed_bytes| values from each renderer. This
// value is safe to be fetched from any sequence.
std::atomic<uint64_t> g_global_reclaimed_bytes{0};

// This is our global swap kill switch.
std::atomic<bool> g_global_swap_allowed{true};

class RendererSwapDataImpl : public RendererSwapData {
 public:
  RendererSwapDataImpl(
      int render_process_host_id,
      base::ProcessId pid,
      std::unique_ptr<UserfaultFD> uffd,
      std::unique_ptr<SwapFile> swap_file,
      const Region& swap_remap_area,
      mojo::PendingRemote<::userspace_swap::mojom::UserspaceSwap>
          pending_remote)
      : render_process_host_id_(render_process_host_id),
        pid_(pid),
        uffd_(std::move(uffd)),
        swap_file_(std::move(swap_file)),
        remote_(std::move(pending_remote)) {
    InitializeSwapRemapAreas(swap_remap_area);
    swap_allowed_ = true;
    VLOG(1) << "Created RendererSwapDataImpl for rphid: "
            << render_process_host_id
            << " Swap Remap Area: " << swap_remap_area;
  }

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

  // Break up the large region which a renderer mmap'ed as PROT_NONE into
  // discrete chunks which can be used for the destinations of an
  // MREMAP_DONTUNMAP.
  void InitializeSwapRemapAreas(const Region& swap_remap_area);

  // AllocFromSwapRegion will find a region which can be used as a destination
  // for a call to MovePTEs. This makes swapping easier, because now we just
  // wait to observe the remap event as our indicator that we can read the
  // memory from the process.
  std::optional<Region> AllocFromSwapRegion();
  void DeallocFromSwapRegion(const Region& region);

  // Swap at most |size_limit| bytes worth of memory on this renderer.
  bool Swap(size_t size_limit);

 private:
  void OnReceivedPASuperPages(
      size_t size_limit_bytes,
      std::vector<::userspace_swap::mojom::MemoryRegionPtr> regions);

  // Convert a mojo MemoryRegionPtr into a vector of userspace swap
  // |resident_regions| (which are of a configurable size) and which are fully
  // resident.
  void PASuperPagesToResidentRegions(
      const Pagemap& pagemap,
      const std::vector<::userspace_swap::mojom::MemoryRegionPtr>& regions,
      std::vector<Region>& resident_regions);

  const int render_process_host_id_;
  const base::ProcessId pid_;

  bool swap_allowed_ = false;

  uint64_t on_disk_bytes_ = 0;
  uint64_t reclaimed_bytes_ = 0;

  // Areas which can be used for moving PTEs.
  std::stack<Region> free_swap_dest_areas_;

  std::unique_ptr<UserfaultFD> uffd_;
  std::unique_ptr<SwapFile> swap_file_;

  // The remote is our link to the renderer to perform the operations it needs
  // to allow for swapping a region.
  mojo::Remote<::userspace_swap::mojom::UserspaceSwap> remote_;

  base::WeakPtrFactory<RendererSwapDataImpl> weak_factory_{this};
};

RendererSwapDataImpl::~RendererSwapDataImpl() = default;

int RendererSwapDataImpl::render_process_host_id() const {
  return render_process_host_id_;
}

void RendererSwapDataImpl::DisallowSwap() {
  swap_allowed_ = false;
}

bool RendererSwapDataImpl::SwapAllowed() const {
  return swap_allowed_ && IsSwapAllowedGlobally();
}

uint64_t RendererSwapDataImpl::SwapDiskspaceWrittenBytes() const {
  return on_disk_bytes_;
}

void RendererSwapDataImpl::InitializeSwapRemapAreas(
    const Region& swap_remap_area) {
  // Break the remap area up into region sized chunks which will be used as
  // the destination of MREMAP_DONTUNMAPs.
  const uint64_t kPagesPerRegion =
      UserspaceSwapConfig::Get().number_of_pages_per_region;
  const size_t kRegionSizeBytes = base::GetPageSize() * kPagesPerRegion;

  for (uint64_t base_addr = swap_remap_area.address;
       (base_addr + kRegionSizeBytes) <=
       (swap_remap_area.address + swap_remap_area.length);
       base_addr += kRegionSizeBytes) {
    free_swap_dest_areas_.push(Region(base_addr, kRegionSizeBytes));
  }
}

uint64_t RendererSwapDataImpl::SwapDiskspaceUsedBytes() const {
  // Because punching a hole may not free the block if the region compressed
  // down to a partial size, the block can only be freed when all of it has
  // been punched. So we will take the larger of what we believe we've written
  // to disk and what the swap file reports as being in use.
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

std::optional<Region> RendererSwapDataImpl::AllocFromSwapRegion() {
  if (free_swap_dest_areas_.empty()) {
    return std::nullopt;
  }

  Region r = free_swap_dest_areas_.top();
  free_swap_dest_areas_.pop();
  return r;
}

void RendererSwapDataImpl::DeallocFromSwapRegion(const Region& region) {
  free_swap_dest_areas_.push(region);
}

void RendererSwapDataImpl::OnReceivedPASuperPages(
    size_t size_limit_bytes,
    std::vector<::userspace_swap::mojom::MemoryRegionPtr> regions) {
  if (regions.empty())
    return;

  // Now that a list of used superpages is available, break it up into region
  // sized chunks which will be checked using the kernel pagemap.
  // Use this processes pagemap to identify regions which are in core.
  Pagemap pagemap(pid_);
  if (!pagemap.IsValid()) {
    // Dont log an error if the process is dead.
    PLOG_IF(ERROR, errno != ENOENT) << "unable to open pagemap";

    // Further swapping is not permitted.
    DisallowSwap();
    return;
  }

  std::vector<Region> resident_regions;

  PASuperPagesToResidentRegions(pagemap, regions, resident_regions);
  if (UserspaceSwapConfig::Get().shuffle_maps_on_swap) {
    // The regions can be shuffled to avoid always swapping the same regions.
    base::ranges::shuffle(resident_regions, std::default_random_engine());
  }

  if (VLOG_IS_ON(1)) {
    uint64_t total_size = 0;
    for (const auto& r : regions) {
      total_size += r.get()->length;
    }

    VLOG(1) << "Pid: " << pid_ << " got " << regions.size()
            << " memory areas totaling " << (total_size >> 20)
            << " MB the round swappable size limit is: "
            << (size_limit_bytes >> 20) << " MB which contained "
            << resident_regions.size() << " resident regions";
  }

  // TODO: Actually do the swap on the regions that have been determined to be
  // resident in the renderer.
}

void RendererSwapDataImpl::PASuperPagesToResidentRegions(
    const Pagemap& pagemap,
    const std::vector<::userspace_swap::mojom::MemoryRegionPtr>& regions,
    std::vector<Region>& resident_regions) {
  // The RegionSize is the size that is considered for swapping, this is
  // independent of PA SuperPage size and is configurable as a multiple of the
  // system page size.
  size_t kRegionSize = UserspaceSwapConfig::Get().number_of_pages_per_region *
                       base::GetPageSize();
  for (const auto& area : regions) {
    Region area_end(area->address + area->length);
    for (Region r(area->address, kRegionSize); r < area_end;
         r.address += kRegionSize) {
      if (!pagemap.IsFullyPresent(r.address, r.length)) {
        continue;
      }

      resident_regions.push_back(r);
    }
  }
}

bool RendererSwapDataImpl::Swap(size_t size_limit_bytes) {
  if (!SwapAllowed()) {
    return false;
  }

  // The first step is always to ask PA what it is using, after that we will
  // check with the kernel to see which of those areas are actually resident.
  remote_->GetPartitionAllocSuperPagesUsed(
      /* max superpages=*/-1,
      base::BindOnce(&RendererSwapDataImpl::OnReceivedPASuperPages,
                     weak_factory_.GetWeakPtr(), size_limit_bytes));

  return true;
}

}  // namespace

UserspaceSwapConfig::UserspaceSwapConfig() = default;
UserspaceSwapConfig::UserspaceSwapConfig(const UserspaceSwapConfig& other) =
    default;

// Static
COMPONENT_EXPORT(USERSPACE_SWAP)
const UserspaceSwapConfig& UserspaceSwapConfig::Get() {
  static UserspaceSwapConfig config = []() -> UserspaceSwapConfig {
    UserspaceSwapConfig config = {};

    // Populate the config object.
    config.enabled = base::FeatureList::IsEnabled(kUserspaceSwap);
    config.number_of_pages_per_region = kUserspaceSwapPagesPerRegion.Get();
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
        base::Seconds(kUserspaceSwapBlockedRefaultTimeSec.Get());
    config.graph_walk_frequency = base::Seconds(
        kUserspaceSwapModeratePressureGraphWalkFrequencySec.Get());
    config.process_swap_frequency =
        base::Seconds(kUserspaceSwapProcessSwapFrequencySec.Get());
    config.invisible_time_before_swap =
        base::Seconds(kUserspaceSwapInvisibleTimeBeforeSwapSec.Get());

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

// KernelSupportsUserspaceSwap will test for all features necessary to enable
// userspace swap.
COMPONENT_EXPORT(USERSPACE_SWAP) bool KernelSupportsUserspaceSwap() {
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) || \
    !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  // We currently only support 64bit PartitionAlloc.
  return false;
#else
  static bool userfault_fd_supported = UserfaultFD::KernelSupportsUserfaultFD();

  // We also need to make sure the kernel supports the mremap operation with
  // MREMAP_DONTUNMAP.
  static bool mremap_dontunmap_supported = []() -> bool {
    const size_t allocation_size = base::GetPageSize();
    void* source_mapping = mmap(nullptr, allocation_size, PROT_NONE,
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
#endif  // !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) ||
        // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
}

RendererSwapData::RendererSwapData() = default;
RendererSwapData::~RendererSwapData() = default;

// static
COMPONENT_EXPORT(USERSPACE_SWAP)
std::unique_ptr<RendererSwapData> RendererSwapData::Create(
    int render_process_host_id,
    base::ProcessId pid,
    std::unique_ptr<UserfaultFD> uffd,
    std::unique_ptr<SwapFile> swap_file,
    const Region& swap_remap_area,
    mojo::PendingRemote<::userspace_swap::mojom::UserspaceSwap> remote) {
  return std::make_unique<RendererSwapDataImpl>(
      render_process_host_id, pid, std::move(uffd), std::move(swap_file),
      swap_remap_area, std::move(remote));
}

COMPONENT_EXPORT(USERSPACE_SWAP) bool UserspaceSwapSupportedAndEnabled() {
  static bool enabled = UserspaceSwapConfig::Get().enabled;
  static bool supported = KernelSupportsUserspaceSwap();
  return supported && enabled;
}

COMPONENT_EXPORT(USERSPACE_SWAP)
bool SwapRenderer(RendererSwapData* data, size_t size_limit_bytes) {
  RendererSwapDataImpl* impl = reinterpret_cast<RendererSwapDataImpl*>(data);
  VLOG(1) << "SwapRenderer for rphid " << impl->render_process_host_id();
  return impl->Swap(size_limit_bytes);
}

COMPONENT_EXPORT(USERSPACE_SWAP)
bool GetPartitionAllocSuperPagesInUse(
    int32_t max_superpages,
    std::vector<::userspace_swap::mojom::MemoryRegionPtr>& regions) {
  regions.clear();
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) || \
    !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  return false;
#else

  uint32_t superpages_remaining =
      max_superpages >= 0 ? max_superpages : UINT32_MAX;

  auto& pool_manager =
      partition_alloc::internal::AddressPoolManager::GetInstance();

  for (partition_alloc::internal::pool_handle ph :
       {partition_alloc::internal::kRegularPoolHandle,
        partition_alloc::internal::kBRPPoolHandle}) {
    uintptr_t pool_base = pool_manager.GetPoolBaseAddress(ph);
    DCHECK(pool_base);

    uintptr_t current_area = 0;
    uint64_t current_area_length = 0;

    std::bitset<partition_alloc::kMaxSuperPagesInPool> alloc_bitset;
    pool_manager.GetPoolUsedSuperPages(ph, alloc_bitset);

    for (size_t i = 0; i < alloc_bitset.size() && superpages_remaining; ++i) {
      if (alloc_bitset.test(i)) {
        superpages_remaining--;
        if (!current_area) {
          current_area = pool_base + (i * partition_alloc::kSuperPageSize);
        }

        current_area_length += partition_alloc::kSuperPageSize;
      } else {
        if (current_area) {
          regions.emplace_back(std::in_place, current_area,
                               current_area_length);
          current_area = 0;
          current_area_length = 0;
        }
      }
    }

    if (current_area) {
      regions.emplace_back(std::in_place, current_area, current_area_length);
    }

    if (!superpages_remaining)
      break;
  }

  return true;
#endif  // !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) ||
        // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
}

COMPONENT_EXPORT(USERSPACE_SWAP) uint64_t GetGlobalMemoryReclaimed() {
  return g_global_reclaimed_bytes.load();
}

COMPONENT_EXPORT(USERSPACE_SWAP) uint64_t GetGlobalSwapDiskspaceUsed() {
  return g_global_disk_usage_bytes.load();
}

COMPONENT_EXPORT(USERSPACE_SWAP) void DisableSwapGlobally() {
  g_global_swap_allowed = false;
}

COMPONENT_EXPORT(USERSPACE_SWAP) bool IsSwapAllowedGlobally() {
  return g_global_swap_allowed;
}

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash
