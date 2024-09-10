// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_RECEIVER_CONFIG_CONVERSIONS_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_RECEIVER_CONFIG_CONVERSIONS_H_

#include "third_party/openscreen/src/cast/streaming/public/receiver_constraints.h"

namespace cast_streaming {

class ReceiverConfig;

openscreen::cast::ReceiverConstraints ToOpenscreenConstraints(
    const ReceiverConfig& config);

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_RECEIVER_CONFIG_CONVERSIONS_H_
