// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Author: Ken Chen <kenchen@google.com> Tiancong Wang <tcwang@google.com>
//
// hugepage text library to remap process executable segment with hugepages.

#include "chromeos/hugepage_text/hugepage_text.h"

#include <link.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "base/bit_cast.h"
#include "base/logging.h"

// CHROMEOS_ORDERFILE_USE is a flag intended to use orderfile
// to link Chrome. The else part of macro check in this code is to
// make sure when the flag is turned off, the code works the same
// as before.
// We plan to turn it on in the future for Chrome OS.
// Therefore, before it's deployed, by default, we turn it off until
// the testing is done.

// These function are here to delimit the start and end of the symbols
// ordered by orderfile.
// Due to ICF (Identical Code Folding), the linker merges functions
// that have the same code (or empty). So we need to give these functions
// some unique body, using inline .word in assembly.
// Note that .word means different sizes in different architectures.
// So we choose 16-bit numbers.
extern "C" {
void chrome_end_ordered_code() {
  asm(".word 0xd44d");
  asm(".word 0xc5b0");
}

void chrome_begin_ordered_code() {
  asm(".word 0xa073");
  asm(".word 0xdda6");
}
}

namespace chromeos {

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000
#endif

#ifndef TMPFS_MAGIC
#define TMPFS_MAGIC 0x01021994
#endif

#ifndef MADV_HUGEPAGE
#define MADV_HUGEPAGE 14
#endif

const base::Feature kCrOSHugepageRemapAndLockZygote{
    "CrOSHugepageRemapAndLockInZygote", base::FEATURE_ENABLED_BY_DEFAULT};

const int kHpageShift = 21;
const int kHpageSize = (1 << kHpageShift);
const int kHpageMask = (~(kHpageSize - 1));

const int kProtection = (PROT_READ | PROT_WRITE);
const int kMremapFlags = (MREMAP_MAYMOVE | MREMAP_FIXED);

// The number of hugepages we want to use to map chrome text section
// to hugepages. Map at least 8 hugepages because of hardware support.
// Map at most 16 hugepages to avoid using too many hugepages.
constexpr static int kMinNumHugePages = 8;
constexpr static int kMaxNumHugePages = 16;

// Get an anonymous mapping backed by explicit transparent hugepage
// Return NULL if such mapping can not be established.
static void* GetTransparentHugepageMapping(const size_t hsize) {
  // setup explicit transparent hugepage segment
  char* addr = static_cast<char*>(mmap(NULL, hsize + kHpageSize, kProtection,
                                       MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
  if (addr == MAP_FAILED) {
    PLOG(INFO) << "unable to mmap anon pages, fall back to small page";
    return NULL;
  }
  // remove unaligned head and tail regions
  size_t head_gap = kHpageSize - (size_t)addr % kHpageSize;
  size_t tail_gap = kHpageSize - head_gap;
  munmap(addr, head_gap);
  munmap(addr + head_gap + hsize, tail_gap);

  void* haddr = addr + head_gap;
  if (madvise(haddr, hsize, MADV_HUGEPAGE)) {
    PLOG(INFO) << "no transparent hugepage support, fall back to small page";
    munmap(haddr, hsize);
    return NULL;
  }

  if (mlock(haddr, hsize)) {
    PLOG(INFO) << "Mlocking text pages failed";
  }
  return haddr;
}

// memcpy for word-aligned data which is not instrumented by AddressSanitizer.
ATTRIBUTE_NO_SANITIZE_ADDRESS
static void NoAsanAlignedMemcpy(void* dst, void* src, size_t size) {
  DCHECK_EQ(0U, size % sizeof(uintptr_t));  // size is a multiple of word size.
  DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(dst) % sizeof(uintptr_t));
  DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(src) % sizeof(uintptr_t));
  uintptr_t* d = reinterpret_cast<uintptr_t*>(dst);
  uintptr_t* s = reinterpret_cast<uintptr_t*>(src);
  for (size_t i = 0; i < size / sizeof(uintptr_t); i++)
    d[i] = s[i];
}

// Remaps text segment at address "vaddr" to hugepage backed mapping via mremap
// syscall.  The virtual address does not change.  When this call returns, the
// backing physical memory will be changed from small page to hugetlb page.
//
// Inputs: vaddr, the starting virtual address to remap to hugepage
//         hsize, size of the memory segment to remap in bytes
// Return: none
// Effect: physical backing page changed from small page to hugepage. If there
//         are error condition, the remapping operation is aborted.
static void MremapHugetlbText(void* vaddr, const size_t hsize) {
  DCHECK_EQ(0ul, reinterpret_cast<uintptr_t>(vaddr) & ~kHpageMask);
  void* haddr = GetTransparentHugepageMapping(hsize);
  if (haddr == NULL)
    return;

  // Copy text segment to hugepage mapping. We are using a non-asan memcpy,
  // otherwise it would be flagged as a bunch of out of bounds reads.
  NoAsanAlignedMemcpy(haddr, vaddr, hsize);

  // change mapping protection to read only now that it has done the copy
  if (mprotect(haddr, hsize, PROT_READ | PROT_EXEC)) {
    PLOG(INFO) << "can not change protection to r-x, fall back to small page";
    munmap(haddr, hsize);
    return;
  }

  // remap hugepage text on top of existing small page mapping
  if (mremap(haddr, hsize, hsize, kMremapFlags, vaddr) == MAP_FAILED) {
    PLOG(INFO) << "unable to mremap hugepage mapping, fall back to small page";
    munmap(haddr, hsize);
    return;
  }
}

// Utility function to get 2MB-aligned address smaller or larger than the
// given address.
// Inputs: address, the address to get 2MB-aligned address for.
//         round_up, whether to get larger (true) or smaller (false) 2MB-aligned
//         address.
// Return: 2MB-aligned address rounded up or down. Or itself if it's
//         already 2MB-aligned.
static size_t RoundToHugepageAlignment(size_t address, bool round_up) {
  // Whether it's round_up or not, if the address is exactly 2MB-aligned,
  // just return itself
  if (address % kHpageSize == 0)
    return address;
  return round_up ? (address / kHpageSize + 1) * kHpageSize
                  : address / kHpageSize * kHpageSize;
}

// Top level text remapping function, when orderfile is enabled.
//
// Inputs: vaddr, the starting virtual address to remap to hugepage
//         segsize, size of the memory segment to remap in bytes
// Return: none
// Effect: physical backing page changed from small page to hugepage. If there
//         are error condition, the remapping operation is aborted.
static void RemapHugetlbTextWithOrderfileLayout(void* vaddr,
                                                const size_t segsize) {
  auto text_start = reinterpret_cast<size_t>(vaddr);
  auto text_end = text_start + segsize;
  auto marker_start = reinterpret_cast<size_t>(chrome_begin_ordered_code);
  auto marker_end = reinterpret_cast<size_t>(chrome_end_ordered_code);
  // Check if the markers are ordered correctly by the orderfile
  if (!(marker_start < marker_end && text_start <= marker_start &&
        marker_end < text_end)) {
    LOG(WARNING) << "The ordering seems incorrect, fall back to small page";
    return;
  }

  // Try to map symbols from the 2MB-aligned address before marker_start
  size_t mapping_start = RoundToHugepageAlignment(marker_start, false);
  if (mapping_start < text_start) {
    // If the address is outside of text section, start to map
    // at the 2MB-aligned address after the marker_start
    mapping_start = RoundToHugepageAlignment(marker_start, true);
  }

  // Try to map symbols to the 2MB-aligned address after the marker_end
  size_t mapping_end = RoundToHugepageAlignment(marker_end, true);
  if (mapping_end > text_end) {
    // If the address is outside of text section, end mapping at
    // the 2MB-aligned address before the marker_end
    // Note that this is not expected to happen for current linker
    // behavior, as the markers are placed in the front (for x86) or
    // are placed in the middle (for ARM/ARM64/PowerPC)
    mapping_end = RoundToHugepageAlignment(marker_end, false);
  }

  size_t hsize = mapping_end - mapping_start;

  // Make sure the number of hugepages used is between kMinNumHugePages
  // and kMaxNumHugePages.
  if (hsize < kHpageSize * kMinNumHugePages) {
    LOG(WARNING) << "Orderfile ordered fewer than " << kMinNumHugePages
                 << " huge pages.";
    hsize = kHpageSize * kMinNumHugePages;
  } else if (hsize > kHpageSize * kMaxNumHugePages) {
    LOG(WARNING) << "Orderfile ordered more than " << kMaxNumHugePages
                 << " huge pages.";
    hsize = kHpageSize * kMaxNumHugePages;
  }

  MremapHugetlbText(reinterpret_cast<void*>(mapping_start), hsize);
}

// Top level text remapping function, without orderfile (old code).
//
// Inputs: vaddr, the starting virtual address to remap to hugepage
//         segsize, size of the memory segment to remap in bytes
// Return: none
// Effect: physical backing page changed from small page to hugepage. If there
//         are error condition, the remaping operation is aborted.
static void RemapHugetlbTextWithoutOrderfileLayout(void* vaddr,
                                                   const size_t segsize) {
  // The number of hugepages to use if no orderfile is specified
  const int kNumHugePages = 15;

  // remove unaligned head regions
  uintptr_t head_gap =
      (kHpageSize - reinterpret_cast<uintptr_t>(vaddr) % kHpageSize) %
      kHpageSize;
  uintptr_t addr = reinterpret_cast<uintptr_t>(vaddr) + head_gap;

  if (segsize < head_gap)
    return;

  size_t hsize = segsize - head_gap;
  hsize = hsize & kHpageMask;

  if (hsize > kHpageSize * kNumHugePages)
    hsize = kHpageSize * kNumHugePages;

  if (hsize == 0)
    return;

  MremapHugetlbText(reinterpret_cast<void*>(addr), hsize);
}

// For a given ELF program header descriptor, iterates over all segments within
// it and find the first segment that has PT_LOAD and is executable, call
// RemapHugetlbText().
//
// Additionally, since these pages are important, we attempt to lock them into
// memory.
//
// Inputs: info: pointer to a struct dl_phdr_info that describes the DSO.
//         size: size of the above structure (not used in this function).
//         data: user param (not used in this function).
// Return: always return true.  The value is propagated by dl_iterate_phdr().
static int FilterElfHeader(struct dl_phdr_info* info, size_t size, void* data) {
  void* vaddr;
  int segsize;

  // From dl_iterate_phdr's man page: "The first object visited by callback is
  // the main program.  For the main program, the dlpi_name field will be an
  // empty string." Hence, no "is this the Chrome we're looking for?" checks are
  // necessary.

  for (int i = 0; i < info->dlpi_phnum; i++) {
    if (info->dlpi_phdr[i].p_type == PT_LOAD &&
        info->dlpi_phdr[i].p_flags == (PF_R | PF_X)) {
      vaddr = bit_cast<void*>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
      segsize = info->dlpi_phdr[i].p_filesz;
      // The following function is conditionally compiled, so use
      // the statements to avoid compiler warnings of unused functions
      (void)RemapHugetlbTextWithOrderfileLayout;
      (void)RemapHugetlbTextWithoutOrderfileLayout;
#ifdef CHROMEOS_ORDERFILE_USE
      RemapHugetlbTextWithOrderfileLayout(vaddr, segsize);
#else
      RemapHugetlbTextWithoutOrderfileLayout(vaddr, segsize);
#endif
      // Only re-map the first text segment.
      return 1;
    }
  }

  return 1;
}

// Main function. This function will iterate all ELF segments, attempt to remap
// parts of the text segment from small page to hugepage, and mlock in all of
// the hugepages. Any errors will cause the failing piece of this to be rolled
// back, so nothing world-ending can come from this function (hopefully ;) ).
void InitHugepagesAndMlockSelf(void) {
  if (base::FeatureList::IsEnabled(chromeos::kCrOSHugepageRemapAndLockZygote)) {
    dl_iterate_phdr(FilterElfHeader, 0);
  }
}

}  // namespace chromeos
