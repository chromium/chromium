// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_aggregator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "components/performance_manager/public/v8_memory/web_memory.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/v8_memory/v8_memory_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace performance_manager {

namespace v8_memory {

using WebMemoryAggregatorPMTest = V8MemoryPerformanceManagerTestHarness;

class WebMemoryAggregatorTest : public GraphTestHarness {
 public:
  // Wrapper for the browsing instance id to improve test readability.
  struct BrowsingInstance {
    int id;
  };

  // Wrapper for memory usage bytes to improve test readability.
  struct Bytes {
    uint64_t bytes;
    bool operator==(const Bytes& other) const { return bytes == other.bytes; }
  };

  void SetUp() override;

  // Creates and adds a new frame node to the graph.
  FrameNodeImpl* AddFrameNode(std::string url,
                              BrowsingInstance,
                              Bytes,
                              FrameNodeImpl* parent = nullptr);

  // Invokes memory measurement and verifies that the result matches the
  // expected memory usage that is provided as a table from a frame URL to
  // bytes.
  void MeasureAndVerify(FrameNodeImpl* frame,
                        base::flat_map<std::string, Bytes> expected);

 private:
  int GetNextUniqueId();
  TestNodeWrapper<ProcessNodeImpl> process_;
  TestNodeWrapper<PageNodeImpl> page_;
  std::vector<TestNodeWrapper<FrameNodeImpl>> frames_;
  int next_unique_id_ = 0;
};

void WebMemoryAggregatorTest::SetUp() {
  process_ = CreateNode<ProcessNodeImpl>();
  page_ = CreateNode<PageNodeImpl>();
}

int WebMemoryAggregatorTest::GetNextUniqueId() {
  return next_unique_id_++;
}

FrameNodeImpl* WebMemoryAggregatorTest::AddFrameNode(
    std::string url,
    BrowsingInstance browsing_instance_id,
    Bytes memory_usage,
    FrameNodeImpl* parent) {
  int frame_tree_node_id = GetNextUniqueId();
  int frame_routing_id = GetNextUniqueId();
  auto frame = CreateNode<FrameNodeImpl>(
      process_.get(), page_.get(), parent, frame_tree_node_id, frame_routing_id,
      blink::LocalFrameToken(), browsing_instance_id.id);
  frame->OnNavigationCommitted(GURL(url), /*same document*/ true);
  V8DetailedMemoryFrameData::CreateForTesting(frame.get())
      ->set_v8_bytes_used(memory_usage.bytes);
  frames_.push_back(std::move(frame));
  return frames_.back().get();
}

void WebMemoryAggregatorTest::MeasureAndVerify(
    FrameNodeImpl* frame,
    base::flat_map<std::string, Bytes> expected) {
  bool measurement_done = false;
  WebMemoryAggregator web_memory(
      frame->frame_token(), V8DetailedMemoryRequest::MeasurementMode::kDefault,
      base::BindLambdaForTesting([&measurement_done, &expected](
                                     mojom::WebMemoryMeasurementPtr result) {
        base::flat_map<std::string, Bytes> actual;
        for (const auto& entry : result->breakdown) {
          EXPECT_EQ(1u, entry->attribution.size());
          EXPECT_EQ(mojom::WebMemoryAttribution::Scope::kWindow,
                    entry->attribution[0]->scope);
          actual[*entry->attribution[0]->url] = Bytes{entry->bytes};
        }
        EXPECT_EQ(expected, actual);
        measurement_done = true;
      }));
  V8DetailedMemoryProcessData process_data;
  web_memory.MeasurementComplete(process_.get(), &process_data);
  EXPECT_TRUE(measurement_done);
}

TEST_F(WebMemoryAggregatorTest, IncludeSameOriginRelatedFrames) {
  auto* main = AddFrameNode("http://foo.com/", BrowsingInstance{0}, Bytes{10u});

  AddFrameNode("http://foo.com/iframe", BrowsingInstance{0}, Bytes{20}, main);

  MeasureAndVerify(main, {
                             {"http://foo.com/", Bytes{10u}},
                             {"http://foo.com/iframe", Bytes{20u}},
                         });
}

TEST_F(WebMemoryAggregatorTest, SkipCrossOriginFrames) {
  auto* main = AddFrameNode("http://foo.com", BrowsingInstance{0}, Bytes{10u});

  AddFrameNode("http://bar.com/iframe", BrowsingInstance{0}, Bytes{20}, main);

  MeasureAndVerify(main, {{"http://foo.com/", Bytes{10u}}});
}

TEST_F(WebMemoryAggregatorTest, SkipUnrelatedFrames) {
  auto* main = AddFrameNode("http://foo.com", BrowsingInstance{0}, Bytes{10u});

  AddFrameNode("http://foo.com/unrelated", BrowsingInstance{1}, Bytes{20});

  MeasureAndVerify(main, {{"http://foo.com/", Bytes{10u}}});
}

TEST_F(WebMemoryAggregatorPMTest, WebMeasureMemory) {
  blink::LocalFrameToken frame_token =
      blink::LocalFrameToken(main_frame()->GetFrameToken());

  // Call WebMeasureMemory on the performance manager sequence and verify that
  // the result matches the data provided by the mock reporter.
  base::RunLoop run_loop;
  auto measurement_callback =
      base::BindLambdaForTesting([&](mojom::WebMemoryMeasurementPtr result) {
        EXPECT_EQ(1u, result->breakdown.size());
        const auto& entry = result->breakdown[0];
        EXPECT_EQ(1u, entry->attribution.size());
        EXPECT_EQ(kMainFrameUrl, *(entry->attribution[0]->url));
        EXPECT_EQ(1001u, entry->bytes);
        run_loop.Quit();
      });

  base::WeakPtr<FrameNode> frame_node_wrapper =
      PerformanceManager::GetFrameNodeForRenderFrameHost(main_frame());
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ASSERT_TRUE(frame_node_wrapper);
        FrameNode* frame_node = frame_node_wrapper.get();
        WebMeasureMemory(frame_node,
                         mojom::WebMemoryMeasurement::Mode::kDefault,
                         std::move(measurement_callback));
      }));

  // Set up and bind the mock reporter.
  MockV8DetailedMemoryReporter mock_reporter;
  {
    auto data = NewPerProcessV8MemoryUsage(1);
    AddIsolateMemoryUsage(frame_token, 1001u, data->isolates[0].get());
    ExpectBindAndRespondToQuery(&mock_reporter, std::move(data),
                                main_process_id());
  }

  // Finally, run all tasks to verify that the memory measurement callback
  // is actually invoked. The test will time out if not.
  run_loop.Run();
}

TEST_F(WebMemoryAggregatorPMTest, MeasurementInterrupted) {
  CreateCrossProcessChildFrame();

  blink::LocalFrameToken frame_token =
      blink::LocalFrameToken(child_frame()->GetFrameToken());

  // Call WebMeasureMemory on the performance manager sequence but delete the
  // process being measured before the result arrives.
  auto measurement_callback =
      base::BindOnce([](mojom::WebMemoryMeasurementPtr result) {
        FAIL() << "Measurement callback ran unexpectedly";
      });

  base::WeakPtr<FrameNode> frame_node_wrapper =
      PerformanceManager::GetFrameNodeForRenderFrameHost(child_frame());
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ASSERT_TRUE(frame_node_wrapper);
        FrameNode* frame_node = frame_node_wrapper.get();
        WebMeasureMemory(frame_node,
                         mojom::WebMemoryMeasurement::Mode::kDefault,
                         std::move(measurement_callback));
      }));

  // Set up and bind the mock reporter.
  MockV8DetailedMemoryReporter mock_reporter;
  {
    ::testing::InSequence seq;
    ExpectBindReceiver(&mock_reporter, child_process_id());

    auto data = NewPerProcessV8MemoryUsage(1);
    AddIsolateMemoryUsage(frame_token, 1001u, data->isolates[0].get());
    ExpectQueryAndDelayReply(&mock_reporter, base::TimeDelta::FromSeconds(10),
                             std::move(data));
  }

  // Verify that requests are sent but reply is not yet received.
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(5));
  ::testing::Mock::VerifyAndClearExpectations(&mock_reporter);

  // Remove the child frame, which will destroy the child process.
  content::RenderFrameHostTester::For(child_frame())->Detach();

  // Advance until the reply is expected to make sure nothing explodes.
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(5));
}

}  // namespace v8_memory

}  // namespace performance_manager
