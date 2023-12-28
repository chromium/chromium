// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_COMMON_PUBLIC_APP_IDS_H_
#define COMPONENTS_CAST_STREAMING_COMMON_PUBLIC_APP_IDS_H_

#include <string_view>

#include "third_party/openscreen/src/cast/common/public/cast_streaming_app_ids.h"

namespace cast_streaming {

// Returns true if |app_id| is associated with a streaming application.
bool IsStreamingReceiverAppId(std::string_view app_id);

// Returns the app ID for the audio and video streaming receiver used by iOS
// apps.
constexpr const char* GetIosAppStreamingAudioVideoAppId() {
  return openscreen::cast::GetIosAppStreamingAudioVideoAppId();
}

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_COMMON_PUBLIC_APP_IDS_H_
