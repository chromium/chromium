// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_DESCRIPTORS_ANDROID_H_
#define CHROME_COMMON_CHROME_DESCRIPTORS_ANDROID_H_

#include "build/build_config.h"
#include "content/public/common/content_descriptors.h"

enum {
  // The main pak file for the user's locale and gender, for strings that are
  // included in WebView. Expected to be sparse.
  kAndroidMainWebViewLocalePakDescriptor = kContentIPCDescriptorMax + 1,

  // The main pak file for the user's locale and gender, for strings that are
  // not included in WebView. Expected to be sparse.
  kAndroidMainNonWebViewLocalePakDescriptor,

  // The pak file to fall back to if a string is not found in the main pak. This
  // one uses the default gender and is expected to be complete. (For strings
  // that are included in WebView).
  kAndroidFallbackWebViewLocalePakDescriptor,

  // The pak file to fall back to if a string is not found in the main pak. This
  // one uses the default gender and is expected to be complete. (For strings
  // that are not included in WebView).
  kAndroidFallbackNonWebViewLocalePakDescriptor,

  kAndroidChrome100PercentPakDescriptor,
  kAndroidUIResourcesPakDescriptor,
  // DFMs with native resources typically do not share file descriptors with
  // child processes. Hence no corresponding *PakDescriptor is defined.
  kAndroidMinidumpDescriptor,
};

#endif  // CHROME_COMMON_CHROME_DESCRIPTORS_ANDROID_H_
