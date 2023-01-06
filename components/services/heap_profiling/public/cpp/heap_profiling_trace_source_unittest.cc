// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/heap_profiling_trace_source.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/producer_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

namespace heap_profiling {
namespace {

class HeapProfilingProtoExporterTest : public testing::Test {
 public:
  void SetUp() override {
    test_handle_ = tracing::PerfettoTracedProcess::SetupForTesting();

    auto perfetto_wrapper = std::make_unique<base::tracing::PerfettoTaskRunner>(
        task_environment_.GetMainThreadTaskRunner());

    producer_ = std::make_unique<tracing::TestProducerClient>(
        std::move(perfetto_wrapper));
  }

  void TearDown() override {
    // Be sure there is no pending/running tasks.
    task_environment_.RunUntilIdle();
  }

  void BeginTrace() {
    HeapProfilingTraceSource::GetInstance()->StartTracing(
        /*data_source_id=*/1, producer_.get(), perfetto::DataSourceConfig());
  }

  void EndTracing() {
    base::RunLoop wait_for_end;
    HeapProfilingTraceSource::GetInstance()->StopTracing(
        wait_for_end.QuitClosure());
    wait_for_end.Run();
  }

  tracing::TestProducerClient* producer() const { return producer_.get(); }

 protected:
  std::unique_ptr<tracing::TestProducerClient> producer_;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<tracing::PerfettoTracedProcess::TestHandle> test_handle_;
};

using Sample = base::SamplingHeapProfiler::Sample;
std::vector<Sample> MakeTestSamples() {
  std::vector<Sample> samples{Sample(10, 100, 1), Sample(5, 102, 2),
                              Sample(7, 103, 3)};
  for (Sample& s : samples) {
    s.allocator =
        base::allocator::dispatcher::AllocationSubsystem::kAllocatorShim;
  }

  void* frame1 = reinterpret_cast<void*>(333);
  void* frame2 = reinterpret_cast<void*>(666);
  void* frame3 = reinterpret_cast<void*>(999);
  samples[0].stack = {frame1};
  samples[1].stack = {frame1, frame2};
  samples[2].stack = {frame3, frame2, frame1};
  return samples;
}

TEST_F(HeapProfilingProtoExporterTest, ProfilingDisabled) {
  BeginTrace();
  EndTracing();
  EXPECT_EQ(producer()->GetFinalizedPacketCount(), 0u);
}

TEST_F(HeapProfilingProtoExporterTest, ProfilingWithoutTracing) {
  auto samples = MakeTestSamples();
  HeapProfilingTraceSource::GetInstance()->AddToTraceIfEnabled(samples);
  BeginTrace();
  EndTracing();
  HeapProfilingTraceSource::GetInstance()->AddToTraceIfEnabled(samples);
  EXPECT_EQ(producer()->GetFinalizedPacketCount(), 0u);
}

TEST_F(HeapProfilingProtoExporterTest, TraceFormat) {
  auto samples = MakeTestSamples();
  BeginTrace();
  HeapProfilingTraceSource::GetInstance()->AddToTraceIfEnabled(samples);
  EndTracing();
  ASSERT_EQ(producer()->GetFinalizedPacketCount(), 5u);

  const auto& packets = producer_->finalized_packets();
  EXPECT_TRUE(packets[0]->incremental_state_cleared());
  EXPECT_FALSE(packets[1]->incremental_state_cleared());
  EXPECT_FALSE(packets[2]->incremental_state_cleared());
  EXPECT_FALSE(packets[3]->incremental_state_cleared());
  EXPECT_FALSE(packets[4]->incremental_state_cleared());

  EXPECT_TRUE(packets[1]->profile_packet().continued());
  EXPECT_TRUE(packets[2]->profile_packet().continued());
  EXPECT_TRUE(packets[3]->profile_packet().continued());
  EXPECT_FALSE(packets[4]->profile_packet().continued());

  EXPECT_EQ(packets[1]->profile_packet().index(), 0u);
  EXPECT_EQ(packets[2]->profile_packet().index(), 1u);
  EXPECT_EQ(packets[3]->profile_packet().index(), 2u);
  EXPECT_EQ(packets[4]->profile_packet().index(), 3u);

  for (unsigned i = 1; i <= 3; ++i) {
    const auto& process_dump = packets[i]->profile_packet().process_dumps(0);
    EXPECT_TRUE(process_dump.has_pid());
    EXPECT_TRUE(process_dump.has_timestamp());
    EXPECT_EQ(process_dump.samples(0).self_allocated(), samples[i - 1].total);
    EXPECT_EQ(process_dump.samples(0).callstack_id(), i);
  }
}

}  // namespace
}  // namespace heap_profiling
