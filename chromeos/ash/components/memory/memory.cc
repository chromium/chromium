// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/memory/memory.h"

#include <link.h>
#include <sys/mman.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/ash/components/memory/elf_sections.h"

namespace ash {

BASE_FEATURE(kCrOSLockMainProgramText,
             "CrOSLockMainProgramText",
             base::FEATURE_DISABLED_BY_DEFAULT);
// The maximum number of bytes that the browser will attempt to lock.
const base::FeatureParam<int> kCrOSLockMainProgramTextMaxSize{
    &kCrOSLockMainProgramText, "CrOSLockMainProgramTextMaxSize",
    32 * 1024 * 1024};

namespace {

// MlockMapping will attempt to lock a mapping using the newer mlock2 (if
// available on kernels 4.4+) with the MLOCK_ONFAULT flag, if the kernel does
// not support it then it will fall back to mlock.
bool MlockMapping(void* addr, size_t size) {
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  int res = mlock2(addr, size, MLOCK_ONFAULT);
  if (res == 0) {
    return true;
  }

  // If the kernel returns ENOSYS it doesn't support mlock2 (pre v4.4) so just
  // fall back to mlock. This is for the case running ash-chrome on linux.
  if (res == -1 && errno != ENOSYS) {
    return false;
  }
#endif
  return mlock(addr, size) == 0;
}

int ParseElfHeaderAndMlockBinaryText(struct dl_phdr_info* info,
                                     size_t size,
                                     void* data) {
  // From dl_iterate_phdr's man page: "The first object visited by callback is
  // the main program.  For the main program, the dlpi_name field will be an
  // empty string." Hence, no "is this the Chrome we're looking for?" checks are
  // necessary.
  for (int i = 0; i < info->dlpi_phnum; i++) {
    if (info->dlpi_phdr[i].p_type == PT_LOAD &&
        info->dlpi_phdr[i].p_flags == (PF_R | PF_X)) {
      uintptr_t vaddr = reinterpret_cast<uintptr_t>(info->dlpi_addr +
                                                    info->dlpi_phdr[i].p_vaddr);
      size_t segsize = info->dlpi_phdr[i].p_filesz;

      ssize_t max_lockable_size = kCrOSLockMainProgramTextMaxSize.Get();
      if (max_lockable_size > -1) {
        // Note mlock/mlock2 do not require a page multiple.
        segsize = std::min(static_cast<ssize_t>(segsize), max_lockable_size);
      }

      if (kRodataAddr == 0 && kTextHotAddr == 0) {
        LOG(WARNING) << "elf section data is not found. mlock first "
                     << segsize / 1024 / 1024 << " MiB";
        PLOG_IF(ERROR, !MlockMapping(reinterpret_cast<void*>(vaddr), segsize))
            << "Unable to lock memory region " << vaddr;
      } else {
        // mlock(2) of Linux allows address and size not being aligned with page
        // size and automatically rounds the address down to the nearest
        // boundary. Since this is ash specific logic, we use the address and
        // the size directly without aligning.
        // https://man7.org/linux/man-pages/man2/mlockall.2.html
        PLOG_IF(ERROR,
                !MlockMapping(reinterpret_cast<void*>(vaddr + kRodataAddr),
                              kRodataSize))
            << "Unable to lock memory region " << vaddr << " for .rodata";
        PLOG_IF(ERROR,
                !MlockMapping(reinterpret_cast<void*>(vaddr + kTextHotAddr),
                              kTextHotSize))
            << "Unable to lock memory region " << vaddr << " for .text.hot";
      }

      return 1;
    }
  }

  return -1;
}

// MlockText will attempt to lock the memory associated with the main program.
void MlockText() {
  int res = dl_iterate_phdr(ParseElfHeaderAndMlockBinaryText, nullptr);
  LOG_IF(ERROR, res == -1)
      << "Unable to lock main program text unable to find entry.";
}

}  // namespace

COMPONENT_EXPORT(ASH_MEMORY) void LockMainProgramText() {
  if (base::FeatureList::IsEnabled(kCrOSLockMainProgramText)) {
    MlockText();
  }
}
}  // namespace ash
