// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_PROTO_UTILS_GOOGLE_RPC_CODE_H_
#define COMPONENTS_LEGION_PROTO_UTILS_GOOGLE_RPC_CODE_H_

#include <string>

#include "components/legion/proto/google_rpc_code.pb.h"

namespace legion {

// An helper function to parse the `legion::GoogleRpcCode` from
// the reason string.
legion::rpc::GoogleRpcCode ParseGoogleRpcCode(const std::string& reason);

}  // namespace legion

#endif  // COMPONENTS_LEGION_PROTO_UTILS_GOOGLE_RPC_CODE_H_
