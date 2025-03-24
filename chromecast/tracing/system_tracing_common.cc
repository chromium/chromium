// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/tracing/system_tracing_common.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include <string.h>

#include <iterator>

namespace chromecast {
namespace tracing {
namespace {

const char kSocketPath[] = "/dev/socket/tracing/tracing";

}  // namespace

sockaddr_un GetSystemTracingSocketAddress() {
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  static_assert(sizeof(kSocketPath) <= sizeof(addr.sun_path),
                "Address too long");
  strncpy(addr.sun_path, kSocketPath, sizeof(addr.sun_path) - 1);
  addr.sun_family = AF_UNIX;
  return addr;
}

}  // namespace tracing
}  // namespace chromecast
