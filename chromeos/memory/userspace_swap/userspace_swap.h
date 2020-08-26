// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_H_
#define CHROMEOS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_H_

#include <sys/mman.h>
#include <cstdint>
#include <vector>

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/memory/userspace_swap/region.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

#ifndef MREMAP_DONTUNMAP
#define MREMAP_DONTUNMAP 4
#endif

// This file is for containing the browser and renderer common userspace swap
// components such as helper functions and structures.
namespace chromeos {
namespace memory {
namespace userspace_swap {

// UserspaceSwapConfig is a structure which contains all configuration values
// for userspace swap.
struct CHROMEOS_EXPORT UserspaceSwapConfig {
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

  // VMA region minimum size represents the minimum size that a VMA must be to
  // be considered for userspace swap.
  uint64_t vma_region_minimum_size_bytes;

  // VMA region maximum size represents the maximum size a VMA can be to be
  // considered for userspace swap. The reason we have a maximum is because it's
  // very common to have LARGE sparse VMAs, which can be 1+GB and used primarily
  // for the on demand page faulted nature of anonymous mappings on linux.
  // Examples of this are how the JVM and others will allocate large several GB
  // heaps in one go. It would be silly to walk the entire thing checking if
  // pages are in core, a good value here is probably less than 256MB.
  uint64_t vma_region_maximum_size_bytes;

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
CHROMEOS_EXPORT bool KernelSupportsUserspaceSwap();

// Returns true if there is kernel support for userspace swap and the feature is
// enabled.
CHROMEOS_EXPORT bool UserspaceSwapSupportedAndEnabled();

// A swap eligible VMA is one that meets the required swapping criteria,
// which are:
//   - RW protections
//   - Not file backed (Anonymous)
//   - Not shared (Private)
//   - Contains no locked memory
//   - Meets the size constraints set by vma_region_min_size_bytes
//     and vma_region_max_size_bytes
CHROMEOS_EXPORT bool IsVMASwapEligible(
    const memory_instrumentation::mojom::VmRegionPtr& vma);

// GetAllSwapEligibleVMAs will return a vector of regions which are swap
// eligible, these regions are NOT "swap region" sized they are the VMAs and as
// such must then be split in the appropriate region size by the userspace swap
// mechanism. On error it will return false and errno will be set appropriately.
//
// This vector may be shuffled if shuffle_maps_on_swap has been set to true.
CHROMEOS_EXPORT bool GetAllSwapEligibleVMAs(base::PlatformThreadId pid,
                                            std::vector<Region>* regions);

}  // namespace userspace_swap
}  // namespace memory
}  // namespace chromeos

#endif  // CHROMEOS_MEMORY_USERSPACE_SWAP_USERSPACE_SWAP_H_
