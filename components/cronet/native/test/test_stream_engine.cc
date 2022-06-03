// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_c.h"

#include "base/check_op.h"
#include "components/cronet/native/test/test_util.h"
#include "components/grpc_support/test/get_stream_engine.h"

namespace grpc_support {

// Provides stream_engine support for testing of bidirectional_stream C API for
// GRPC using native Cronet_Engine.

Cronet_EnginePtr g_cronet_engine = nullptr;
int quic_server_port = 0;

// Returns a stream_engine* for testing with the QuicTestServer.
// The engine returned resolve "test.example.com" as localhost:|port|,
// and should have "test.example.com" configured as a QUIC server.
stream_engine* GetTestStreamEngine(int port) {
  CHECK(g_cronet_engine);
  CHECK_EQ(port, quic_server_port);
  return Cronet_Engine_GetStreamEngine(g_cronet_engine);
}

// Starts the stream_engine* for testing with the QuicTestServer.
// Has the same properties as GetTestStreamEngine.  This function is
// used when the stream_engine* needs to be shut down and restarted
// between test cases (including between all of the bidirectional
// stream test cases and all other tests for the engine; this is the
// situation for Cronet).
void StartTestStreamEngine(int port) {
  CHECK(!g_cronet_engine);
  quic_server_port = port;
  g_cronet_engine = cronet::test::CreateTestEngine(port);
}

// Shuts a stream_engine* started with |StartTestStreamEngine| down.
// See comment above.
void ShutdownTestStreamEngine() {
  CHECK(g_cronet_engine);
  Cronet_Engine_Destroy(g_cronet_engine);
  g_cronet_engine = nullptr;
  quic_server_port = 0;
}

}  // namespace grpc_support
