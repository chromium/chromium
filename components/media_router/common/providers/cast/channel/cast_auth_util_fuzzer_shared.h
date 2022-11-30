// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_AUTH_UTIL_FUZZER_SHARED_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_AUTH_UTIL_FUZZER_SHARED_H_

#include <string>
#include <vector>
#include "components/media_router/common/providers/cast/channel/fuzz_proto/fuzzer_inputs.pb.h"

namespace cast_channel {
namespace fuzz {

// Potentially updates |input| before it should be used for fuzzing.  For
// example, if an auth message is present in the proto, it will override
// |cast_message|.
void SetupAuthenticateChallengeReplyInput(
    const std::vector<std::string>& certs,
    CastAuthUtilInputs::AuthenticateChallengeReplyInput* input);

}  // namespace fuzz
}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_AUTH_UTIL_FUZZER_SHARED_H_
