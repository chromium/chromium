// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_TRACING_SYSTEM_TRACING_COMMON_H_
#define CHROMECAST_TRACING_SYSTEM_TRACING_COMMON_H_

#include <sys/socket.h>
#include <sys/un.h>

namespace chromecast {
namespace tracing {

extern const char* const kCategories[];

extern const size_t kCategoryCount;

sockaddr_un GetSystemTracingSocketAddress();

}  // namespace tracing
}  // namespace chromecast

#endif  // CHROMECAST_TRACING_SYSTEM_TRACING_COMMON_H_
