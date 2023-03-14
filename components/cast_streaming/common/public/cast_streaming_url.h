// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_COMMON_PUBLIC_CAST_STREAMING_URL_H_
#define COMPONENTS_CAST_STREAMING_COMMON_PUBLIC_CAST_STREAMING_URL_H_

#include "url/gurl.h"

namespace cast_streaming {

// Returns the Cast Streaming media source URL.
GURL GetCastStreamingMediaSourceUrl();

// Returns true if |url| is the Cast Streaming media source URL.
bool IsCastStreamingMediaSourceUrl(const GURL& url);

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_COMMON_PUBLIC_CAST_STREAMING_URL_H_
