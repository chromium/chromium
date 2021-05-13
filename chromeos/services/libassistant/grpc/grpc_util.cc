// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/grpc_util.h"

#include "base/check_op.h"

namespace chromeos {
namespace libassistant {

grpc_local_connect_type GetGrpcLocalConnectType(
    const std::string& server_address) {
  // We only support unix socket on our platform.
  DCHECK_EQ(server_address.compare(0, 4, "unix"), 0);
  return UDS;
}

}  // namespace libassistant
}  // namespace chromeos
