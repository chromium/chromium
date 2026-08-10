// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PROTO_UTILS_CLIENT_METADATA_UTILS_H_
#define COMPONENTS_PRIVATE_AI_PROTO_UTILS_CLIENT_METADATA_UTILS_H_

#include "components/private_ai/proto/private_ai.pb.h"
#include "components/version_info/channel.h"

namespace private_ai {

// Creates and populates a ClientMetadata proto with ChromeClientMetadata.
proto::ClientMetadata CreateClientMetadata(version_info::Channel channel);

// Converts version_info::Channel to proto::ChromeClientMetadata::Channel.
proto::ChromeClientMetadata::Channel ConvertChannelToProto(
    version_info::Channel channel);

// Returns the proto::ChromeClientMetadata::Platform for current build/OS.
proto::ChromeClientMetadata::Platform GetPlatformForProto();

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_PROTO_UTILS_CLIENT_METADATA_UTILS_H_
