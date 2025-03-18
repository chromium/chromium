// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <string>

namespace syncer {

std::string GetPersonalizableDeviceNameInternal() {
  char hostname[HOST_NAME_MAX];
  if (gethostname(hostname, std::size(hostname)) == 0)  // Success.
    return std::string(hostname, strnlen(hostname, std::size(hostname)));
  return std::string();
}

}  // namespace syncer
