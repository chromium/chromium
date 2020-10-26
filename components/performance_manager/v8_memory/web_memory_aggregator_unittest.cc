// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_aggregator.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/v8_memory/v8_detailed_memory.h"
#include "components/performance_manager/public/v8_memory/web_memory.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace performance_manager {

namespace v8_memory {

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
      frame->frame_token(),
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

}  // namespace v8_memory

}  // namespace performance_manager
