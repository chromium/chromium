// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/compiler_specific.h"

#if defined(_WIN32)
#define PR_EXPORT __declspec(dllexport)
#else
#define PR_EXPORT __attribute__((visibility("default")))
#endif

extern "C" PR_EXPORT const char* GetLibraryName() {
  return "platform_runtime_test_lib";
}

extern "C" PR_EXPORT DISABLE_CFI_DLSYM bool ProcessRequestHeaders(
    void* headers,
    bool (*get_header)(void*, const char*, char*, size_t),
    void (*set_header)(void*, const char*, const char*),
    const char* url) {
  if (!headers || !get_header || !set_header) {
    return false;
  }
  char host[256] = {};
  if (!get_header(headers, "host", host, sizeof(host))) {
    return false;
  }
  set_header(headers, host, url);
  return true;
}
