// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_CAST_CORE_SWITCHES_H_
#define CHROMECAST_CAST_CORE_CAST_CORE_SWITCHES_H_

namespace cast::core::switches {

// Specifies the Cast Core runtime ID, --cast-core-runtime-id=<runtime_id>.
inline constexpr char kCastCoreRuntimeId[] = "cast-core-runtime-id";

// Specifies the Cast Core runtime gRPC endpoint,
// --runtime-service-path=<endpoint>.
inline constexpr char kRuntimeServicePath[] = "runtime-service-path";

// Specifies the Cast Core service gRPC endpoint.
inline constexpr char kCastCoreServiceEndpoint[] = "cast-core-service-endpoint";

// Specifies that TCP/IP should be used as the gRPC transport typeby Cast Core;
// otherwise UDS is used.
inline constexpr char kEnableGrpcOverTcpIp[] = "enable-grpc-over-tcpip";

// Authentication token securely sent to the runtime.
inline constexpr char kRuntimeAuthToken[] = "runtime-auth-token";

}  // namespace cast::core::switches

#endif  // CHROMECAST_CAST_CORE_CAST_CORE_SWITCHES_H_
