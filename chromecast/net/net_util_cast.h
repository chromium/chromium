// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_NET_UTIL_CAST_H_
#define CHROMECAST_NET_NET_UTIL_CAST_H_

#include <string>
#include <unordered_set>

namespace chromecast {

// Gets the list of interfaces that should be ignored. The interfaces returned
// by this function will not be used to connect to the internet.
std::unordered_set<std::string> GetIgnoredInterfaces();

}  // namespace chromecast

#endif  // CHROMECAST_NET_NET_UTIL_CAST_H_
