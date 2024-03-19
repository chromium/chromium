// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_c.h"

#include "base/check_op.h"
#include "components/cronet/native/test/test_util.h"
#include "components/grpc_support/test/get_stream_engine.h"

namespace grpc_support {

namespace {

// Provides stream_engine support for testing of bidirectional_stream C API for
// GRPC using native Cronet_Engine.
class TestStreamEngineGetterImpl : public TestStreamEngineGetter {
 public:
  explicit TestStreamEngineGetterImpl(int port)
      : cronet_engine_(cronet::test::CreateTestEngine(port)) {
    CHECK(cronet_engine_);
  }

  ~TestStreamEngineGetterImpl() override {
    Cronet_Engine_Destroy(cronet_engine_);
  }

  stream_engine* Get() override {
    return Cronet_Engine_GetStreamEngine(cronet_engine_);
  }

 private:
  Cronet_EnginePtr cronet_engine_;
};

}  // namespace

// WARNING: An alternative implementation of Create() exists in
// //components/grpc_support/test/get_stream_engine.cc. They are never both
// linked into the same binary.

// static
std::unique_ptr<TestStreamEngineGetter> TestStreamEngineGetter::Create(
    int port) {
  return std::make_unique<TestStreamEngineGetterImpl>(port);
}

}  // namespace grpc_support
