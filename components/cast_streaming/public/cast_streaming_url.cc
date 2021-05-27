// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/public/cast_streaming_url.h"

#include "url/gurl.h"

namespace cast_streaming {
namespace {

// TODO(crbug.com/1211062): Update this constant.
constexpr char kCastStreamingReceiverUrl[] = "data:cast_streaming_receiver";

}  // namespace

bool IsCastStreamingMediaSourceUrl(const GURL& url) {
  return url == kCastStreamingReceiverUrl;
}

}  // namespace cast_streaming
