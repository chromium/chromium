// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_UTIL_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_UTIL_H_

#include "third_party/grpc/src/include/grpc/grpc_security_constants.h"

#include <string>

namespace chromeos {
namespace libassistant {

// Returns the local connection type for the given server address.
grpc_local_connect_type GetGrpcLocalConnectType(
    const std::string& server_address);

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_GRPC_UTIL_H_
