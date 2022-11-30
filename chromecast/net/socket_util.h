// Copyright 2021 The Chromium Authors
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

// Creates a socket pair and returns true if succeeded.
bool CreateUnnamedSocketPair(base::ScopedFD* fd1, base::ScopedFD* fd2);

// Returns a net::StreamSocket wrapping the |socket_fd|.
// Note: nullptr will be returned if |socket_fd| is invalid or an error occurred
// when adopting the socket descriptor.
std::unique_ptr<net::StreamSocket> AdoptUnnamedSocketHandle(
    base::ScopedFD socket_fd);

}  // namespace chromecast

#endif  // CHROMECAST_NET_SOCKET_UTIL_H_
