// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_METRICS_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_METRICS_H_

#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"

namespace cast_channel {

// Records the flags set on a Cast channel when a Cast LAUNCH message is sent.
void RecordLaunchSessionChannelFlags(CastChannelFlags flags);

}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_METRICS_H_
