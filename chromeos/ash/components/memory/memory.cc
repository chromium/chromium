// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/memory.h"

#include <link.h>
#include <sys/mman.h>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

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
      void* vaddr =
          reinterpret_cast<void*>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
      size_t segsize = info->dlpi_phdr[i].p_filesz;

      ssize_t max_lockable_size = kCrOSLockMainProgramTextMaxSize.Get();
      if (max_lockable_size > -1) {
        // Note mlock/mlock2 do not require a page multiple.
        segsize = std::min(static_cast<ssize_t>(segsize), max_lockable_size);
      }

      PLOG_IF(ERROR, MlockMapping(vaddr, segsize) != 0)
          << "Unable to lock memory region " << vaddr;
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
