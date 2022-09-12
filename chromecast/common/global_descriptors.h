// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_GLOBAL_DESCRIPTORS_H_
#define CHROMECAST_COMMON_GLOBAL_DESCRIPTORS_H_

#include "build/build_config.h"
#include "content/public/common/content_descriptors.h"

// This is a list of global descriptor keys to be used with the
// base::GlobalDescriptors object (see base/posix/global_descriptors.h)
enum {
  // TODO(gunsch): Remove once there's a real value here. Otherwise, non-Android
  // build compile fails due to empty enum.
  kDummyValue = kContentIPCDescriptorMax + 1,
#if BUILDFLAG(IS_ANDROID)
  kAndroidPakDescriptor,
  kAndroidMinidumpDescriptor,
#endif  // BUILDFLAG(IS_ANDROID)
};

#endif  // CHROMECAST_COMMON_GLOBAL_DESCRIPTORS_H_
