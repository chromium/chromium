// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_TEST_TEST_UTIL_H_
#define COMPONENTS_CRONET_NATIVE_TEST_TEST_UTIL_H_

#include "base/functional/callback.h"
#include "cronet_c.h"

namespace cronet {
// Various test utility functions for testing Cronet.
namespace test {

// Create an engine that is configured to support local test servers.
Cronet_EnginePtr CreateTestEngine(int quic_server_port);

// Create an executor that runs tasks on different background thread.
Cronet_ExecutorPtr CreateTestExecutor();

// Class to wrap Cronet_Runnable into a base::OnceClosure.
class RunnableWrapper {
 public:
  ~RunnableWrapper() { Cronet_Runnable_Destroy(runnable_); }

  // Wrap a Cronet_Runnable into a base::OnceClosure.
  static base::OnceClosure CreateOnceClosure(Cronet_RunnablePtr runnable);

 private:
  friend std::unique_ptr<RunnableWrapper> std::make_unique<RunnableWrapper>(
      Cronet_RunnablePtr&);

  explicit RunnableWrapper(Cronet_RunnablePtr runnable) : runnable_(runnable) {}

  void Run() { Cronet_Runnable_Run(runnable_); }

  const Cronet_RunnablePtr runnable_;
};

}  // namespace test
}  // namespace cronet

#endif  // COMPONENTS_CRONET_NATIVE_TEST_TEST_UTIL_H_
