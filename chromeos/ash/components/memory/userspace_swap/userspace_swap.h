// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_H_

#include <sys/mman.h>
#include <cstdint>
#include <vector>

#include "base/component_export.h"
#include "base/process/process_handle.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chromeos/ash/components/memory/userspace_swap/region.h"
#include "chromeos/ash/components/memory/userspace_swap/userspace_swap.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

#ifndef MREMAP_DONTUNMAP
#define MREMAP_DONTUNMAP 4
#endif

namespace userspace_swap {
namespace mojom {
class UserspaceSwap;
}  // namespace mojom
}  // namespace userspace_swap

// This file is for containing the browser and renderer common userspace swap
// components such as helper functions and structures.
namespace ash {
namespace memory {
namespace userspace_swap {

class UserfaultFD;
class SwapFile;

// UserspaceSwapConfig is a structure which contains all configuration values
// for userspace swap.
struct COMPONENT_EXPORT(USERSPACE_SWAP) UserspaceSwapConfig {
  UserspaceSwapConfig();
  UserspaceSwapConfig(const UserspaceSwapConfig& other);

  // Returns the Current UserspaceSwapConfig.
  static const UserspaceSwapConfig& Get();
  friend std::ostream& operator<<(std::ostream& out,
                                  const UserspaceSwapConfig& c);

  // enabled is true if the userspace swap feature is enabled.
  bool enabled;

  // Number of pages per region represents the number of pages we will use for
  // each chunk we attempt to swap at a time.
  uint16_t number_of_pages_per_region;

  // If true the swap file will be compressed on disk.
  bool use_compressed_swap_file;

  // Minimum disk space swap available is the lower limit of free disk space on
  // the swap device. If the available space on the device backing storage
  // is lower than this value no further swapping is allowed, this prevents
  // userspace swap from exhausting disk space.
  uint64_t minimum_swap_disk_space_available;

  // Maximum swap disk space represents the maxmium disk space userspace swap is
  // allowed to use across all renderers.
  uint64_t maximum_swap_disk_space_bytes;

  // Renderer maximum disk file size represents the maximum size a swap file may
  // for an individual renderer.
  uint64_t renderer_maximum_disk_swap_file_size_bytes;

  // Renderer region limit per swap limits the number of regions that that an
  // individual renderer can swap on each swap, note that each region is
  // configured by the number of pages per region so these two together limit
  // the total number of pages per swap round of a process.
  uint32_t renderer_region_limit_per_swap;

  // The blocked refault time is the minimum time a region must be swapped out
  // without being blocked. This prevents disk thrashing where if a region
  // is immediately refaulted we don't want to swap it again as it'll likely be
  // needed in the future, for example, if a region has a blocked refault time
  // of 30s if it is refaulted in less than 30s it will never be swapped again.
  base::TimeDelta blocked_refault_time;

  // Graph walk frequency represents the (shortest) duration in which you can
  // walk the graph, that is, a graph walk frequency of 60s means that you will
  // not walk the graph more than once every 60s.
  base::TimeDelta graph_walk_frequency;

  // The process Swap frequency limits the frequency on which a process may be
  // swapped, for example 60s means that a process will not be swapped more than
  // once every 60s.
  base::TimeDelta process_swap_frequency;

  // Invisible Time Before Swap is the amount of time a renderer must be
  // invisible before it can be considered for swap.
  base::TimeDelta invisible_time_before_swap;

  // Swap on freeze, if true will swap a process when all frames are frozen.
  bool swap_on_freeze;

  // Swap on moderate pressure will walk the graph (based on the frequency of
  // graph walk frequency) looking for renderers to swap based on visibility
  // state.
  bool swap_on_moderate_pressure;

  // Shuffle maps order will randomly shuffle the processes maps ordering before
  // swapping, it does this so that subsequent swaps can start from different
  // memory regions.
  bool shuffle_maps_on_swap;
};

// Returns true if the kernel supports all the features necessary for userspace
// swap. These features are userfaultfd(2) and mremap(2) with MREMAP_DONTUNMAP
// support this method is the source of truth for the browser UserspaceSwap and
// the rendererer UserspaceSwapImpl.
COMPONENT_EXPORT(USERSPACE_SWAP) bool KernelSupportsUserspaceSwap();

// Returns true if there is kernel support for userspace swap and the feature is
// enabled.
COMPONENT_EXPORT(USERSPACE_SWAP) bool UserspaceSwapSupportedAndEnabled();

// GetGlobalSwapDiskspaceUsed returns the number of bytes currently on disk for
// ALL renderers.
COMPONENT_EXPORT(USERSPACE_SWAP) uint64_t GetGlobalSwapDiskspaceUsed();

// GetGlobalMemoryReclaimed returns the number of bytes (physical memory) which
// has been reclaimed by userspace swap. This number may not match what is on
// disk due to encryption and compression.
COMPONENT_EXPORT(USERSPACE_SWAP) uint64_t GetGlobalMemoryReclaimed();

// DisableSwapGlobally is the global swap kill switch, it prevents any further
// swapping.
COMPONENT_EXPORT(USERSPACE_SWAP) void DisableSwapGlobally();

// Returns true if swap is allowed (globally).
COMPONENT_EXPORT(USERSPACE_SWAP) bool IsSwapAllowedGlobally();

// RendererSwapData is attached to a ProcessNode and owned by the ProcessNode on
// the PerformanceManager graph.
class COMPONENT_EXPORT(USERSPACE_SWAP) RendererSwapData {
 public:
  virtual ~RendererSwapData();

  static std::unique_ptr<RendererSwapData> Create(
      int render_process_host_id,
      base::ProcessId pid,
      std::unique_ptr<UserfaultFD> uffd,
      std::unique_ptr<SwapFile> swap_file,
      const Region& swap_remap_area,
      mojo::PendingRemote<::userspace_swap::mojom::UserspaceSwap> remote);

  // Returns the Render Process Host ID associated with this RendererSwapData.
  virtual int render_process_host_id() const = 0;

  // If true this renderer has not been disallowed swap.
  virtual bool SwapAllowed() const = 0;

  // DisallowSwap prevents further swapping of this renderer. This cannot be
  // unset.
  virtual void DisallowSwap() = 0;

  // There is a subtle difference between SwapdiskspaceWrittenBytes and
  // SwapDiskspaceUsedBytes. Because punching a hole in a file may not reclaim a
  // block on disk only after the entire block has been punched will the space
  // actually be reclaimed on disk. However, SwapDiskspaceWrittenBytes will
  // contain the total number of bytes that we think are on disk, these numbers
  // will be equal when there is no waste of block space on disk.
  virtual uint64_t SwapDiskspaceWrittenBytes() const = 0;
  virtual uint64_t SwapDiskspaceUsedBytes() const = 0;

  virtual uint64_t ReclaimedBytes() const = 0;

 protected:
  RendererSwapData();
};

// SwapRenderer will initiate a swap on the renderer belonging to the
// RendererSwapData |data|. |size_limit_bytes| is a limit imposed by the system
// based on settings.
COMPONENT_EXPORT(USERSPACE_SWAP)
bool SwapRenderer(RendererSwapData* data, size_t size_limit_bytes);

// GetPartitionAllocSuperPagesInUse will return |max_superpages| worth of
// regions that are currently allocated by PartitionAlloc.
COMPONENT_EXPORT(USERSPACE_SWAP)
bool GetPartitionAllocSuperPagesInUse(
    int32_t max_superpages,
    std::vector<::userspace_swap::mojom::MemoryRegionPtr>& regions);

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_H_
