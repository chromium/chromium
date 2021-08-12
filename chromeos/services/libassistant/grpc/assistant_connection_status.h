// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CONNECTION_STATUS_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CONNECTION_STATUS_H_

namespace chromeos {
namespace libassistant {

// Status of the grpc connection.
enum class AssistantConnectionStatus {
  // Connection to the assistant's gRPC server is not available. The assistant
  // is either booting up or restarting after a crash.
  OFFLINE,

  // All statuses indicating that the assistant's gRPC server is online:
  //
  // Once online, the statuses here are monotonic. The status may transition
  // back to OFFLINE though at any time (ex: libassistant crashes).
  //
  // Communication with the assistant process has initiated. Only a few
  // core gRPC services are available in the assistant process. All rpcs to
  // these core bootup services are made internally within AssistantClient.
  // AssistantClient's caller need not worry about interpreting this status for
  // the purposes of making RPCs; it's only an indication that the assistant
  // process is alive.
  ONLINE_BOOTING_UP,
  // The assistant process is fully initialized, and all of its services are
  // available for calling.
  ONLINE_ALL_SERVICES_AVAILABLE
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CONNECTION_STATUS_H_
