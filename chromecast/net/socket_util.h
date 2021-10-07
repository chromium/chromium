// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_SOCKET_UTIL_H_
#define CHROMECAST_NET_SOCKET_UTIL_H_

#include <memory>

#include "base/files/scoped_file.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace chromecast {

std::unique_ptr<net::StreamSocket> AdoptUnnamedSocketHandle(
    base::ScopedFD socket_fd);

}  // namespace chromecast

#endif  // CHROMECAST_NET_SOCKET_UTIL_H_
