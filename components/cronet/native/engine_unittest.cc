// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cronet_c.h"

#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "components/cronet/native/engine.h"
#include "components/cronet/native/generated/cronet.idl_impl_struct.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {

namespace {

// Fake sent byte count for metrics testing.
constexpr int64_t kSentByteCount = 12345;

// App implementation of Cronet_Executor methods.
void TestExecutor_Execute(Cronet_ExecutorPtr self, Cronet_RunnablePtr command) {
  CHECK(self);
  Cronet_Runnable_Run(command);
  Cronet_Runnable_Destroy(command);
}

// Context for TestRequestInfoListener_OnRequestFinished().
using TestOnRequestFinishedClientContext = int;

// App implementation of Cronet_RequestFinishedInfoListener methods.
//
// Expects a client context of type TestOnRequestFinishedClientContext -- will
// increment this value.
void TestRequestInfoListener_OnRequestFinished(
    Cronet_RequestFinishedInfoListenerPtr self,
    Cronet_RequestFinishedInfoPtr request_info,
    Cronet_UrlResponseInfoPtr url_response_info,
    Cronet_ErrorPtr error) {
  CHECK(self);
  Cronet_ClientContext context =
      Cronet_RequestFinishedInfoListener_GetClientContext(self);
  auto* listener_run_count =
      static_cast<TestOnRequestFinishedClientContext*>(context);
  ++(*listener_run_count);
  auto* metrics = Cronet_RequestFinishedInfo_metrics_get(request_info);
  EXPECT_EQ(kSentByteCount, Cronet_Metrics_sent_byte_count_get(metrics));
  EXPECT_NE(nullptr, url_response_info);
  EXPECT_NE(nullptr, error);
}

TEST(EngineUnitTest, HasNoRequestFinishedInfoListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, HasRequestFinishedInfoListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_RequestFinishedInfoListenerPtr listener =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  Cronet_ExecutorPtr executor =
      Cronet_Executor_CreateWith(TestExecutor_Execute);
  Cronet_Engine_AddRequestFinishedListener(engine, listener, executor);

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_TRUE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_RemoveRequestFinishedListener(engine, listener);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());
  Cronet_Executor_Destroy(executor);
  Cronet_RequestFinishedInfoListener_Destroy(listener);
  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, RequestFinishedInfoListeners) {
  using RequestInfo = base::RefCountedData<Cronet_RequestFinishedInfo>;
  using UrlResponseInfo = base::RefCountedData<Cronet_UrlResponseInfo>;
  using CronetError = base::RefCountedData<Cronet_Error>;
  constexpr int kNumListeners = 5;
  TestOnRequestFinishedClientContext listener_run_count = 0;

  Cronet_EnginePtr engine = Cronet_Engine_Create();
  Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();

  Cronet_RequestFinishedInfoListenerPtr listeners[kNumListeners];
  Cronet_ExecutorPtr executor =
      Cronet_Executor_CreateWith(TestExecutor_Execute);
  for (int i = 0; i < kNumListeners; ++i) {
    listeners[i] = Cronet_RequestFinishedInfoListener_CreateWith(
        TestRequestInfoListener_OnRequestFinished);
    Cronet_RequestFinishedInfoListener_SetClientContext(listeners[i],
                                                        &listener_run_count);
    Cronet_Engine_AddRequestFinishedListener(engine, listeners[i], executor);
  }

  // Simulate the UrlRequest reporting metrics to the engine.
  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  auto request_info = base::MakeRefCounted<RequestInfo>();
  auto url_response_info = base::MakeRefCounted<UrlResponseInfo>();
  auto error = base::MakeRefCounted<CronetError>();
  auto metrics = std::make_unique<Cronet_Metrics>();
  metrics->sent_byte_count = kSentByteCount;
  request_info->data.metrics.emplace(*metrics);
  engine_impl->ReportRequestFinished(request_info, url_response_info, error);
  EXPECT_EQ(kNumListeners, listener_run_count);

  for (auto* listener : listeners) {
    Cronet_RequestFinishedInfoListener_Destroy(listener);
    Cronet_Engine_RemoveRequestFinishedListener(engine, listener);
  }
  Cronet_Executor_Destroy(executor);
  Cronet_Engine_Destroy(engine);
  Cronet_EngineParams_Destroy(engine_params);
}

TEST(EngineUnitTest, AddNullRequestFinishedInfoListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_ExecutorPtr executor =
      Cronet_Executor_CreateWith(TestExecutor_Execute);
  EXPECT_DCHECK_DEATH_WITH(
      Cronet_Engine_AddRequestFinishedListener(engine, nullptr, executor),
      "Both listener and executor must be non-null. listener: .* executor: "
      ".*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_Executor_Destroy(executor);
  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, AddNullRequestFinishedInfoExecutor) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_RequestFinishedInfoListenerPtr listener =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  EXPECT_DCHECK_DEATH_WITH(
      Cronet_Engine_AddRequestFinishedListener(engine, listener, nullptr),
      "Both listener and executor must be non-null. listener: .* executor: "
      ".*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_RequestFinishedInfoListener_Destroy(listener);
  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, AddNullRequestFinishedInfoListenerAndExecutor) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  EXPECT_DCHECK_DEATH_WITH(
      Cronet_Engine_AddRequestFinishedListener(engine, nullptr, nullptr),
      "Both listener and executor must be non-null. listener: .* executor: "
      ".*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, AddListenerTwice) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_RequestFinishedInfoListenerPtr listener =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  Cronet_ExecutorPtr executor =
      Cronet_Executor_CreateWith(TestExecutor_Execute);
  Cronet_Engine_AddRequestFinishedListener(engine, listener, executor);
  EXPECT_DCHECK_DEATH_WITH(
      Cronet_Engine_AddRequestFinishedListener(engine, listener, executor),
      "Listener .* already registered with executor .*, \\*NOT\\* changing to "
      "new executor .*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_TRUE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_RemoveRequestFinishedListener(engine, listener);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());
  Cronet_Executor_Destroy(executor);
  Cronet_RequestFinishedInfoListener_Destroy(listener);
  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, RemoveNonexistentListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_RequestFinishedInfoListenerPtr listener =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  EXPECT_DCHECK_DEATH_WITH(
      Cronet_Engine_RemoveRequestFinishedListener(engine, listener),
      "Asked to erase non-existent RequestFinishedInfoListener .*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_RequestFinishedInfoListener_Destroy(listener);
  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, RemoveNonexistentListenerWithAddedListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_RequestFinishedInfoListenerPtr listener =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  Cronet_RequestFinishedInfoListenerPtr listener2 =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  Cronet_ExecutorPtr executor =
      Cronet_Executor_CreateWith(TestExecutor_Execute);
  Cronet_Engine_AddRequestFinishedListener(engine, listener, executor);

  EXPECT_DCHECK_DEATH_WITH(
      Cronet_Engine_RemoveRequestFinishedListener(engine, listener2),
      "Asked to erase non-existent RequestFinishedInfoListener .*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_TRUE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_RemoveRequestFinishedListener(engine, listener);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());
  Cronet_RequestFinishedInfoListener_Destroy(listener);
  Cronet_RequestFinishedInfoListener_Destroy(listener2);
  Cronet_Executor_Destroy(executor);
  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, RemoveNullListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  EXPECT_DCHECK_DEATH_WITH(
      Cronet_Engine_RemoveRequestFinishedListener(engine, nullptr),
      "Asked to erase non-existent RequestFinishedInfoListener .*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_Destroy(engine);
}

}  // namespace
}  // namespace cronet
