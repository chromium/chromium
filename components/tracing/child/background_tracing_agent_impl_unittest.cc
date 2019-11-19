// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/child/background_tracing_agent_impl.h"

#include "base/run_loop.h"

#include "base/metrics/histogram_macros.h"
#include "base/test/task_environment.h"
#include "components/tracing/child/background_tracing_agent_provider_impl.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

class BackgroundTracingAgentClientRecorder
    : public tracing::mojom::BackgroundTracingAgentClient {
 public:
  void OnInitialized() override { ++on_initialized_count_; }

  void OnTriggerBackgroundTrace(const std::string& histogram_name) override {
    ++on_trigger_background_trace_count_;
    on_trigger_background_trace_histogram_name_ = histogram_name;
  }

  void OnAbortBackgroundTrace() override { ++on_abort_background_trace_count_; }

  int on_initialized_count() const { return on_initialized_count_; }
  int on_trigger_background_trace_count() const {
    return on_trigger_background_trace_count_;
  }
  int on_abort_background_trace_count() const {
    return on_abort_background_trace_count_;
  }

  const std::string& on_trigger_background_trace_histogram_name() const {
    return on_trigger_background_trace_histogram_name_;
  }

 private:
  int on_initialized_count_ = 0;
  int on_trigger_background_trace_count_ = 0;
  int on_abort_background_trace_count_ = 0;
  std::string on_trigger_background_trace_histogram_name_;
};

class BackgroundTracingAgentImplTest : public testing::Test {
 public:
  BackgroundTracingAgentImplTest() {
    provider_set_.Add(
        std::make_unique<tracing::BackgroundTracingAgentProviderImpl>(),
        provider_.BindNewPipeAndPassReceiver());

    auto recorder = std::make_unique<BackgroundTracingAgentClientRecorder>();
    recorder_ = recorder.get();

    mojo::PendingRemote<tracing::mojom::BackgroundTracingAgentClient> client;
    client_set_.Add(std::move(recorder),
                    client.InitWithNewPipeAndPassReceiver());

    provider_->Create(0, std::move(client),
                      agent_.BindNewPipeAndPassReceiver());
  }

  tracing::mojom::BackgroundTracingAgent* agent() { return agent_.get(); }

  BackgroundTracingAgentClientRecorder* recorder() const { return recorder_; }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<tracing::mojom::BackgroundTracingAgentProvider> provider_;
  mojo::Remote<tracing::mojom::BackgroundTracingAgent> agent_;
  mojo::UniqueReceiverSet<tracing::mojom::BackgroundTracingAgentProvider>
      provider_set_;
  mojo::UniqueReceiverSet<tracing::mojom::BackgroundTracingAgentClient>
      client_set_;
  BackgroundTracingAgentClientRecorder* recorder_ = nullptr;
};

TEST_F(BackgroundTracingAgentImplTest, TestInitialize) {
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, recorder()->on_initialized_count());
}

TEST_F(BackgroundTracingAgentImplTest, TestHistogramDoesNotTrigger) {
  LOCAL_HISTOGRAM_COUNTS("foo1", 10);

  agent()->SetUMACallback("foo1", 20000, 25000, true);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, recorder()->on_initialized_count());
  EXPECT_EQ(0, recorder()->on_trigger_background_trace_count());
  EXPECT_EQ(0, recorder()->on_abort_background_trace_count());
}

TEST_F(BackgroundTracingAgentImplTest, TestHistogramTriggers) {
  LOCAL_HISTOGRAM_COUNTS("foo2", 2);

  agent()->SetUMACallback("foo2", 1, 3, true);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, recorder()->on_initialized_count());
  EXPECT_EQ(1, recorder()->on_trigger_background_trace_count());
  EXPECT_EQ(0, recorder()->on_abort_background_trace_count());
  EXPECT_EQ("foo2", recorder()->on_trigger_background_trace_histogram_name());
}

TEST_F(BackgroundTracingAgentImplTest, TestHistogramAborts) {
  LOCAL_HISTOGRAM_COUNTS("foo3", 10);

  agent()->SetUMACallback("foo3", 1, 3, false);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, recorder()->on_initialized_count());
  EXPECT_EQ(0, recorder()->on_trigger_background_trace_count());
  EXPECT_EQ(1, recorder()->on_abort_background_trace_count());
}

}  // namespace tracing
