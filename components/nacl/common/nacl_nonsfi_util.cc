// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_nonsfi_util.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/nacl/common/nacl_switches.h"

namespace nacl {

bool IsNonSFIModeEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH) && \
    (defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARMEL))
  return true;
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNaClNonSfiMode);
#else
  return false;
#endif
}

}  // namespace nacl
