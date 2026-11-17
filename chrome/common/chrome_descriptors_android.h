// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_DESCRIPTORS_ANDROID_H_
#define CHROME_COMMON_CHROME_DESCRIPTORS_ANDROID_H_

#include "build/build_config.h"
#include "content/public/common/content_descriptors.h"

enum {
  // The main pak file for the user's locale and gender. Expected to be sparse.
  kAndroidMainLocalePakDescriptor = kContentIPCDescriptorMax + 1,

  // The pak file to fall back to if a string is not found in the main pak. This
  // one uses the default gender and is expected to be complete.
  kAndroidFallbackLocalePakDescriptor,

  kAndroidChrome100PercentPakDescriptor,
  kAndroidChrome200PercentPakDescriptor,
  kAndroidUIResourcesPakDescriptor,
  // DFMs with native resources typically do not share file descriptors with
  // child processes. Hence no corresponding *PakDescriptor is defined.
  kAndroidMinidumpDescriptor,
};

#endif  // CHROME_COMMON_CHROME_DESCRIPTORS_ANDROID_H_
