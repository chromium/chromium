// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GRPC_SUPPORT_TEST_GET_STREAM_ENGINE_H_
#define COMPONENTS_GRPC_SUPPORT_TEST_GET_STREAM_ENGINE_H_

#include <memory>

extern "C" typedef struct stream_engine stream_engine;

namespace grpc_support {

class TestStreamEngineGetter {
 public:
  // Starts the stream_engine for testing with the QuicTestServer. The
  // stream_engine is owned by this object and should not be used after
  // destruction. The engine returned resolves kTestServerHost as
  // localhost:|port|, and has kTestServerHost configured as a QUIC server.
  static std::unique_ptr<TestStreamEngineGetter> Create(int port);

  TestStreamEngineGetter() = default;

  // Base class. Prevent slicing.
  TestStreamEngineGetter(const TestStreamEngineGetter&) = delete;
  TestStreamEngineGetter& operator=(const TestStreamEngineGetter&) = delete;

  // Cleanly shuts down the stream_engine.
  virtual ~TestStreamEngineGetter() = default;

  // Returns the stream_engine.
  virtual stream_engine* Get() = 0;
};

}  // namespace grpc_support

#endif  // COMPONENTS_GRPC_SUPPORT_TEST_GET_STREAM_ENGINE_H_
