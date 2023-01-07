// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GRPC_SUPPORT_TEST_GET_STREAM_ENGINE_H_
#define COMPONENTS_GRPC_SUPPORT_TEST_GET_STREAM_ENGINE_H_

struct stream_engine;

namespace grpc_support {

// Returns a stream_engine* for testing with the QuicTestServer.
// The engine returned should resolve kTestServerHost as localhost:|port|,
// and should have kTestServerHost configured as a QUIC server.
stream_engine* GetTestStreamEngine(int port);

// Starts the stream_engine* for testing with the QuicTestServer.
// Has the same properties as GetTestStreamEngine.  This function is
// used when the stream_engine* needs to be shut down and restarted
// between test cases (including between all of the bidirectional
// stream test cases and all other tests for the engine; this is the
// situation for Cronet).
void StartTestStreamEngine(int port);

// Shuts a stream_engine* started with |StartTestStreamEngine| down.
// See comment above.
void ShutdownTestStreamEngine();

}  // namespace grpc_support

#endif  // COMPONENTS_GRPC_SUPPORT_TEST_GET_STREAM_ENGINE_H_
