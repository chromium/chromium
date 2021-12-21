// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_COMMON_CORS_EXEMPT_HEADERS_H_
#define CHROMECAST_CAST_CORE_RUNTIME_COMMON_CORS_EXEMPT_HEADERS_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"

namespace chromecast {

// Returns the list of CORS exempt headers for Cast Core.
const std::vector<std::string>& GetCastCoreCorsExemptHeadersList();

// Verifies if a |header| is CORS exempt using case-insensitive comparison.
bool IsHeaderCorsExempt(base::StringPiece header);

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_COMMON_CORS_EXEMPT_HEADERS_H_
