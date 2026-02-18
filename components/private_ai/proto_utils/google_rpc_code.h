// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PROTO_UTILS_GOOGLE_RPC_CODE_H_
#define COMPONENTS_PRIVATE_AI_PROTO_UTILS_GOOGLE_RPC_CODE_H_

#include <string>

#include "components/private_ai/proto/google_rpc_code.pb.h"

namespace private_ai {

// An helper function to parse the `private_ai::GoogleRpcCode` from
// the reason string.
rpc::GoogleRpcCode ParseGoogleRpcCode(const std::string& reason);

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_PROTO_UTILS_GOOGLE_RPC_CODE_H_
