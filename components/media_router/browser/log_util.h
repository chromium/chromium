// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_LOG_UTIL_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_LOG_UTIL_H_

#include <string_view>

namespace media_router::log_util {

// Gets the first nine characters of an ID string.
//
// For Cast and DIAL sink IDs, this happens to return the cast: or dial: prefix
// and the first four characters of the UUID.
std::string_view TruncateId(std::string_view id);

}  // namespace media_router::log_util

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_LOG_UTIL_H_
