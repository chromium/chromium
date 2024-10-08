// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_DESCRIPTORS_H_
#define CHROME_COMMON_CHROME_DESCRIPTORS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/common/content_descriptors.h"

enum {
#if BUILDFLAG(IS_ANDROID)
  kAndroidLocalePakDescriptor = kContentIPCDescriptorMax + 1,
  kAndroidSecondaryLocalePakDescriptor,
  kAndroidChrome100PercentPakDescriptor,
  kAndroidUIResourcesPakDescriptor,
  // DFMs with native resources typically do not share file descriptors with
  // child processes. Hence no corresponding *PakDescriptor is defined.
  kAndroidMinidumpDescriptor,

#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  kCrosStartupDataDescriptor = kContentIPCDescriptorMax + 1,
#endif
};

#endif  // CHROME_COMMON_CHROME_DESCRIPTORS_H_
