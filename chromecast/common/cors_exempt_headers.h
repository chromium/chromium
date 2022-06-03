// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_CORS_EXEMPT_HEADERS_H_
#define CHROMECAST_COMMON_CORS_EXEMPT_HEADERS_H_

#include "base/containers/span.h"

namespace chromecast {

// Returns the list of existing headers which pre-date CORS preflight check
// support in HTTP servers.
// TODO(b/154337552): Remove this list once all the servers support CORS
// preflight requests.
base::span<const char*> GetLegacyCorsExemptHeaders();

}  // namespace chromecast

#endif  // CHROMECAST_COMMON_CORS_EXEMPT_HEADERS_H_
