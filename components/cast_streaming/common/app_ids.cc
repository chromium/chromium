// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/common/public/app_ids.h"

namespace cast_streaming {

bool IsStreamingReceiverAppId(base::StringPiece app_id) {
  const std::string app_id_string(app_id.data(), app_id.length());
  return openscreen::cast::IsCastStreamingReceiverAppId(app_id_string);
}

}  // namespace cast_streaming
