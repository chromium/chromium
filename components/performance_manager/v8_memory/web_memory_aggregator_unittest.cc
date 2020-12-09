// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_aggregator.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/trace_event/traced_value.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/mojom/v8_contexts.mojom.h"
#include "components/performance_manager/public/mojom/web_memory.mojom.h"
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

using AttributionScope = mojom::WebMemoryAttribution::Scope;
using NodeAggregationType = WebMemoryAggregator::NodeAggregationType;

using WebMemoryAggregatorPMTest = V8MemoryPerformanceManagerTestHarness;

class WebMemoryAggregatorTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  // Wrapper for memory usage bytes to improve test readability.
  struct Bytes {
    uint64_t bytes;
    bool operator==(const Bytes& other) const { return bytes == other.bytes; }
  };

  void SetUp() override;

  // Creates and adds a new frame node to the graph.
  FrameNodeImpl* AddFrameNode(
      std::string url,
      Bytes bytes,
      FrameNodeImpl* parent = nullptr,
      base::Optional<std::string> id_attribute = base::nullopt,
      base::Optional<std::string> src_attribute = base::nullopt) {
    return AddFrameNodeImpl(url, kDefaultBrowsingInstanceId, bytes, parent,
                            /*opener=*/nullptr, id_attribute, src_attribute);
  }

  // Creates a frame node as if from window.open and adds it to the graph.
  FrameNodeImpl* AddFrameNodeFromOpener(std::string url,
                                        Bytes bytes,
                                        FrameNodeImpl* opener) {
    return AddFrameNodeImpl(url, kDefaultBrowsingInstanceId, bytes,
                            /*parent=*/nullptr, opener);
  }

  // Creates a frame node in a different browsing instance and adds it to the
  // graph.
  FrameNodeImpl* AddCrossBrowsingInstanceFrameNode(
      std::string url,
      Bytes bytes,
      FrameNodeImpl* parent = nullptr,
      base::Optional<std::string> id_attribute = base::nullopt,
      base::Optional<std::string> src_attribute = base::nullopt) {
    return AddFrameNodeImpl(url, kDefaultBrowsingInstanceId + 1, bytes, parent,
                            /*opener=*/nullptr, id_attribute, src_attribute);
  }

  // Creates a frame node in a different browsing instance as if from
  // window.open and adds it to the graph.
  FrameNodeImpl* AddCrossBrowsingInstanceFrameNodeFromOpener(
      std::string url,
      Bytes bytes,
      FrameNodeImpl* opener) {
    return AddFrameNodeImpl(url, kDefaultBrowsingInstanceId + 1, bytes,
                            /*parent=*/nullptr, opener);
  }

  // Invokes memory measurement and verifies that the result matches the
  // expected memory usage that is provided as a table from a frame URL to
  // bytes.
  void MeasureAndVerify(FrameNodeImpl* frame,
                        base::flat_map<std::string, Bytes> expected);

 private:
  static constexpr int kDefaultBrowsingInstanceId = 0;

  // Creates and adds a new frame node to the graph.
  FrameNodeImpl* AddFrameNodeImpl(
      std::string url,
      int browsing_instance_id,
      Bytes bytes,
      FrameNodeImpl* parent = nullptr,
      FrameNodeImpl* opener = nullptr,
      base::Optional<std::string> id_attribute = base::nullopt,
      base::Optional<std::string> src_attribute = base::nullopt);
  int GetNextUniqueId();
  TestNodeWrapper<ProcessNodeImpl> process_;
  std::vector<TestNodeWrapper<PageNodeImpl>> pages_;
  std::vector<TestNodeWrapper<FrameNodeImpl>> frames_;
  int next_unique_id_ = 0;
};

void WebMemoryAggregatorTest::SetUp() {
  GetGraphFeaturesHelper().EnableV8ContextTracker();
  Super::SetUp();
  process_ = CreateNode<ProcessNodeImpl>();
  pages_.push_back(CreateNode<PageNodeImpl>());
}

int WebMemoryAggregatorTest::GetNextUniqueId() {
  return next_unique_id_++;
}

FrameNodeImpl* WebMemoryAggregatorTest::AddFrameNodeImpl(
    std::string url,
    int browsing_instance_id,
    Bytes memory_usage,
    FrameNodeImpl* parent,
    FrameNodeImpl* opener,
    base::Optional<std::string> id_attribute,
    base::Optional<std::string> src_attribute) {
  // If there's an opener, the new frame is also a new page.
  auto* page = pages_.front().get();
  if (opener) {
    pages_.push_back(CreateNode<PageNodeImpl>());
    page = pages_.back().get();
    page->SetOpenerFrameNodeAndOpenedType(opener, PageNode::OpenedType::kPopup);
  }

  int frame_tree_node_id = GetNextUniqueId();
  int frame_routing_id = GetNextUniqueId();
  auto frame_token = blink::LocalFrameToken();
  auto frame = CreateNode<FrameNodeImpl>(process_.get(), page, parent,
                                         frame_tree_node_id, frame_routing_id,
                                         frame_token, browsing_instance_id);
  frame->OnNavigationCommitted(GURL(url), /*same document*/ true);
  V8DetailedMemoryExecutionContextData::CreateForTesting(frame.get())
      ->set_v8_bytes_used(memory_usage.bytes);
  frames_.push_back(std::move(frame));
  FrameNodeImpl* frame_impl = frames_.back().get();

  // Create a V8ContextDescription with attribution data for this frame. (In
  // production this is done by PerformanceManager monitoring frame lifetime
  // events.)
  auto description = mojom::V8ContextDescription::New();
  description->token = blink::V8ContextToken();
  description->world_type = mojom::V8ContextWorldType::kMain;
  description->execution_context_token = frame_token;

  mojom::IframeAttributionDataPtr attribution;
  if (parent) {
    // Frame attribution attributes come from the frame's parent node, so
    // V8ContextTracker expects an IframeAttributionData. The attribute values
    // may be empty.
    attribution = mojom::IframeAttributionData::New();
    attribution->id = id_attribute;
    attribution->src = src_attribute;
  } else {
    // V8ContextTracker expects no IframeAttributionData.
    DCHECK(!id_attribute);
    DCHECK(!src_attribute);
  }
  DCHECK(frame_impl->process_node());
  frame_impl->process_node()->OnV8ContextCreated(std::move(description),
                                                 std::move(attribution));

  return frame_impl;
}

void WebMemoryAggregatorTest::MeasureAndVerify(
    FrameNodeImpl* frame,
    base::flat_map<std::string, Bytes> expected) {
  bool measurement_done = false;
  WebMemoryMeasurer web_memory(
      frame->frame_token(), V8DetailedMemoryRequest::MeasurementMode::kDefault,
      base::BindLambdaForTesting([&measurement_done, &expected](
                                     mojom::WebMemoryMeasurementPtr result) {
        base::flat_map<std::string, Bytes> actual;
        for (const auto& entry : result->breakdown) {
          EXPECT_EQ(1u, entry->attribution.size());
          EXPECT_EQ(AttributionScope::kWindow, entry->attribution[0]->scope);
          actual[*entry->attribution[0]->url] = Bytes{entry->bytes};
        }
        EXPECT_EQ(expected, actual);
        measurement_done = true;
      }));
  V8DetailedMemoryProcessData process_data;
  web_memory.MeasurementComplete(process_.get(), &process_data);
  EXPECT_TRUE(measurement_done);
}

struct ExpectedMemoryBreakdown {
  uint64_t bytes = 0U;
  AttributionScope scope = AttributionScope::kWindow;
  base::Optional<std::string> url;
  base::Optional<std::string> id;
  base::Optional<std::string> src;

  ExpectedMemoryBreakdown() = default;
  ExpectedMemoryBreakdown(
      uint64_t expected_bytes,
      AttributionScope expected_scope,
      base::Optional<std::string> expected_url = base::nullopt,
      base::Optional<std::string> expected_id = base::nullopt,
      base::Optional<std::string> expected_src = base::nullopt)
      : bytes(expected_bytes),
        scope(expected_scope),
        url(std::move(expected_url)),
        id(std::move(expected_id)),
        src(std::move(expected_src)) {}

  ExpectedMemoryBreakdown(const ExpectedMemoryBreakdown& other) = default;
  ExpectedMemoryBreakdown& operator=(const ExpectedMemoryBreakdown& other) =
      default;
};

mojom::WebMemoryMeasurementPtr CreateExpectedMemoryMeasurement(
    const std::vector<ExpectedMemoryBreakdown>& breakdowns) {
  auto expected_measurement = mojom::WebMemoryMeasurement::New();
  for (const auto& breakdown : breakdowns) {
    auto expected_breakdown = mojom::WebMemoryBreakdownEntry::New();
    expected_breakdown->bytes = breakdown.bytes;

    auto attribution = mojom::WebMemoryAttribution::New();
    attribution->scope = breakdown.scope;
    attribution->url = breakdown.url;
    attribution->id = breakdown.id;
    attribution->src = breakdown.src;
    expected_breakdown->attribution.push_back(std::move(attribution));

    expected_measurement->breakdown.push_back(std::move(expected_breakdown));
  }
  return expected_measurement;
}

// Abuse Mojo's trace integration to serialize a measurement to sorted JSON for
// string comparison. This gives failure messages that include the full
// measurement in JSON format and is easier than comparing every field of
// nested Mojo messages individually.
std::string MeasurementToJSON(
    const mojom::WebMemoryMeasurementPtr& measurement) {
  // Sort all arrays.
  auto canonical_measurement = measurement->Clone();
  for (const auto& breakdown_entry : canonical_measurement->breakdown) {
    std::sort(breakdown_entry->attribution.begin(),
              breakdown_entry->attribution.end());
  }
  std::sort(canonical_measurement->breakdown.begin(),
            canonical_measurement->breakdown.end());

  // Convert to JSON string.
  base::trace_event::TracedValueJSON json_value;
  canonical_measurement->AsValueInto(&json_value);
  return json_value.ToJSON();
}

TEST_F(WebMemoryAggregatorTest, MeasurerIncludesSameOriginRelatedFrames) {
  auto* main = AddFrameNode("http://foo.com/", Bytes{10u});

  AddFrameNode("http://foo.com/iframe", Bytes{20}, main);

  MeasureAndVerify(main, {
                             {"http://foo.com/", Bytes{10u}},
                             {"http://foo.com/iframe", Bytes{20u}},
                         });
}

// TODO(b/1085129): Currently WebMemoryMeasurer only includes the results for a
// single process. Once it invokes WebMemoryAggregator, update this test to
// expect cross-origin frames to be included in the aggregation.
TEST_F(WebMemoryAggregatorTest, MeasurerSkipsCrossOriginFrames) {
  auto* main = AddFrameNode("http://foo.com", Bytes{10u});

  AddFrameNode("http://bar.com/iframe", Bytes{20}, main);

  MeasureAndVerify(main, {{"http://foo.com/", Bytes{10u}}});
}

TEST_F(WebMemoryAggregatorTest, MeasurerSkipsCrossBrowserContextGroupFrames) {
  auto* main = AddFrameNode("http://foo.com", Bytes{10u});

  AddCrossBrowsingInstanceFrameNode("http://foo.com/unrelated", Bytes{20});

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

TEST_F(WebMemoryAggregatorTest, CreateBreakdownEntry) {
  auto measurement = mojom::WebMemoryMeasurement::New();
  auto* breakdown_with_no_url =
      internal::CreateBreakdownEntry(AttributionScope::kCrossOriginAggregated,
                                     base::nullopt, measurement.get());
  auto* breakdown_with_url = internal::CreateBreakdownEntry(
      AttributionScope::kWindow, "https://example.com", measurement.get());
  auto* breakdown_with_empty_url = internal::CreateBreakdownEntry(
      AttributionScope::kWindow, "", measurement.get());

  // Ensure breakdowns were added to measurement.
  EXPECT_EQ(measurement->breakdown.size(), 3U);
  EXPECT_EQ(measurement->breakdown[0].get(), breakdown_with_no_url);
  EXPECT_EQ(measurement->breakdown[1].get(), breakdown_with_url);
  EXPECT_EQ(measurement->breakdown[2].get(), breakdown_with_empty_url);

  // Can't use an initializer list because nullopt_t and
  // base::Optional<std::string> are different types.
  std::vector<base::Optional<std::string>> attributes;
  attributes.push_back(base::nullopt);
  attributes.push_back(base::make_optional("example_attr"));
  attributes.push_back(base::make_optional(""));
  for (const auto& attribute : attributes) {
    SCOPED_TRACE(attribute.value_or("nullopt"));

    // V8ContextTracker needs a parent frame to store attributes.
    FrameNodeImpl* parent_frame =
        attribute ? AddFrameNode("https://example.com", Bytes{1}, nullptr)
                  : nullptr;
    FrameNodeImpl* frame = AddFrameNode("https://example.com", Bytes{1},
                                        parent_frame, attribute, attribute);
    internal::SetBreakdownAttributionFromFrame(frame, breakdown_with_url);
    internal::CopyBreakdownAttribution(breakdown_with_url,
                                       breakdown_with_empty_url);

    // All measurements should be created with 0 bytes.
    auto expected_result = CreateExpectedMemoryMeasurement({
        ExpectedMemoryBreakdown(0, AttributionScope::kCrossOriginAggregated,
                                /*expected_url=*/base::nullopt,
                                /*expected_id=*/base::nullopt,
                                /*expected_src=*/base::nullopt),
        ExpectedMemoryBreakdown(0, AttributionScope::kWindow,
                                "https://example.com", attribute, attribute),
        ExpectedMemoryBreakdown(0, AttributionScope::kWindow,
                                /*expected_url=*/"", attribute, attribute),
    });
    EXPECT_EQ(MeasurementToJSON(measurement),
              MeasurementToJSON(expected_result));
  }
}

TEST_F(WebMemoryAggregatorTest, AggregateSingleFrame) {
  // Example 1 from http://wicg.github.io/performance-measure-memory/#examples
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
  });
  EXPECT_EQ(internal::FindAggregationStartNode(main_frame), main_frame);
  WebMemoryAggregator aggregator(main_frame);
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(MeasurementToJSON(result), MeasurementToJSON(expected_result));
}

TEST_F(WebMemoryAggregatorTest, AggregateSingleSiteMultiFrame) {
  // Example 2 from http://wicg.github.io/performance-measure-memory/#examples
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  FrameNodeImpl* child_frame =
      AddFrameNode("https://example.com/iframe.html", Bytes{5}, main_frame,
                   "example-id", "redirect.html?target=iframe.html");

  EXPECT_EQ(internal::FindAggregationStartNode(main_frame), main_frame);
  WebMemoryAggregator aggregator(main_frame);

  // Test the relationships of each node in the graph.
  EXPECT_EQ(aggregator.FindNodeAggregationType(main_frame),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(aggregator.FindNodeAggregationType(child_frame),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                child_frame, aggregator.requesting_origin()),
            main_frame);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(5, AttributionScope::kWindow,
                              "https://example.com/iframe.html", "example-id",
                              "redirect.html?target=iframe.html"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(MeasurementToJSON(result), MeasurementToJSON(expected_result));
}

TEST_F(WebMemoryAggregatorTest, AggregateCrossOrigin) {
  // Example 5 from http://wicg.github.io/performance-measure-memory/#examples
  //
  // example.com (10 bytes)
  // |
  // *--foo.com/iframe1 (5 bytes)
  //      |
  //      *--foo.com/iframe2 (2 bytes)
  //      |
  //      *--bar.com/iframe2 (3 bytes)
  //      |
  //      *--foo.com/worker.js (4 bytes)
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  FrameNodeImpl* child_frame =
      AddFrameNode("https://foo.com/iframe1", Bytes{5}, main_frame,
                   "example-id", "https://foo.com/iframe1");
  FrameNodeImpl* grandchild1 =
      AddFrameNode("https://foo.com/iframe2", Bytes{2}, child_frame,
                   "example-id2", "https://foo.com/iframe2");
  FrameNodeImpl* grandchild2 =
      AddFrameNode("https://bar.com/iframe2", Bytes{3}, child_frame,
                   "example-id3", "https://bar.com/iframe2");
  // TODO(crbug.com/1085129): In the spec this is a worker, but they're not
  // supported yet.
  FrameNodeImpl* grandchild3 =
      AddFrameNode("https://foo.com/worker.js", Bytes{4}, child_frame);

  EXPECT_EQ(internal::FindAggregationStartNode(main_frame), main_frame);
  WebMemoryAggregator aggregator(main_frame);

  // Test the relationships of each node in the graph.
  EXPECT_EQ(aggregator.FindNodeAggregationType(main_frame),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(aggregator.FindNodeAggregationType(child_frame),
            NodeAggregationType::kCrossOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                child_frame, aggregator.requesting_origin()),
            main_frame);
  EXPECT_EQ(aggregator.FindNodeAggregationType(grandchild1),
            NodeAggregationType::kCrossOriginAggregated);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                grandchild1, aggregator.requesting_origin()),
            nullptr);
  EXPECT_EQ(aggregator.FindNodeAggregationType(grandchild2),
            NodeAggregationType::kCrossOriginAggregated);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                grandchild2, aggregator.requesting_origin()),
            nullptr);
  EXPECT_EQ(aggregator.FindNodeAggregationType(grandchild3),
            NodeAggregationType::kCrossOriginAggregated);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                grandchild3, aggregator.requesting_origin()),
            nullptr);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(14, AttributionScope::kCrossOriginAggregated,
                              base::nullopt, "example-id",
                              "https://foo.com/iframe1"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(MeasurementToJSON(result), MeasurementToJSON(expected_result));
}

TEST_F(WebMemoryAggregatorTest, AggregateNestedCrossOrigin) {
  // Based on example 6 from
  // http://wicg.github.io/performance-measure-memory/#examples with some
  // further nested frames added to test all combinations of same-origin &
  // cross-origin children & parents.
  //
  // example.com (10 bytes)
  // |
  // *--foo.com/iframe1 (5 bytes)  <-- opaque to requesting node
  //      |
  //      *--bar.com/iframe1 (4 bytes)  <-- invisible to requesting node
  //           |
  //           *--example.com/iframe1 (3 bytes)
  //              |
  //              *--foo.com/iframe2 (2 bytes)  <-- opaque to requesting node
  //              |  |
  //              |  *--example.com/iframe2 (1 byte)
  //              |
  //              *--example.com/iframe3 (6 bytes)
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  FrameNodeImpl* subframe =
      AddFrameNode("https://foo.com/iframe1", Bytes{5}, main_frame,
                   "example-id", "https://foo.com/iframe1");
  FrameNodeImpl* subframe2 =
      AddFrameNode("https://bar.com/iframe1", Bytes{4}, subframe, "example-id2",
                   "https://bar.com/iframe1");
  FrameNodeImpl* subframe3 =
      AddFrameNode("https://example.com/iframe1", Bytes{3}, subframe2,
                   "example-id3", "https://example.com/iframe1");
  FrameNodeImpl* subframe4 =
      AddFrameNode("https://foo.com/iframe2", Bytes{2}, subframe3,
                   "example-id4", "https://foo.com/iframe2");
  FrameNodeImpl* subframe5 =
      AddFrameNode("https://example.com/iframe2", Bytes{1}, subframe4,
                   "example-id5", "https://example.com/iframe2");
  FrameNodeImpl* subframe6 =
      AddFrameNode("https://example.com/iframe3", Bytes{6}, subframe3,
                   "example-id6", "https://example.com/iframe3");

  // A frame with 0 bytes of memory use (eg. a frame that's added to the frame
  // tree during the measurement) should not appear in the result.
  FrameNodeImpl* empty_frame =
      AddFrameNode("https://example.com/empty_frame", Bytes{0}, subframe3);

  EXPECT_EQ(internal::FindAggregationStartNode(main_frame), main_frame);
  WebMemoryAggregator aggregator(main_frame);

  // Test the relationships of each node in the graph.
  EXPECT_EQ(aggregator.FindNodeAggregationType(main_frame),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(aggregator.FindNodeAggregationType(subframe),
            NodeAggregationType::kCrossOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                subframe, aggregator.requesting_origin()),
            main_frame);
  EXPECT_EQ(aggregator.FindNodeAggregationType(subframe2),
            NodeAggregationType::kCrossOriginAggregated);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                subframe2, aggregator.requesting_origin()),
            nullptr);
  EXPECT_EQ(aggregator.FindNodeAggregationType(subframe3),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                subframe3, aggregator.requesting_origin()),
            nullptr);
  EXPECT_EQ(aggregator.FindNodeAggregationType(subframe4),
            NodeAggregationType::kCrossOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                subframe4, aggregator.requesting_origin()),
            subframe3);
  EXPECT_EQ(aggregator.FindNodeAggregationType(subframe5),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                subframe5, aggregator.requesting_origin()),
            nullptr);
  EXPECT_EQ(aggregator.FindNodeAggregationType(subframe6),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                subframe6, aggregator.requesting_origin()),
            subframe3);
  EXPECT_EQ(aggregator.FindNodeAggregationType(empty_frame),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                empty_frame, aggregator.requesting_origin()),
            subframe3);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(9, AttributionScope::kCrossOriginAggregated,
                              base::nullopt, "example-id",
                              "https://foo.com/iframe1"),
      ExpectedMemoryBreakdown(3, AttributionScope::kWindow,
                              "https://example.com/iframe1", "example-id",
                              "https://foo.com/iframe1"),
      ExpectedMemoryBreakdown(2, AttributionScope::kCrossOriginAggregated,
                              base::nullopt, "example-id4",
                              "https://foo.com/iframe2"),
      ExpectedMemoryBreakdown(1, AttributionScope::kWindow,
                              "https://example.com/iframe2", "example-id4",
                              "https://foo.com/iframe2"),
      ExpectedMemoryBreakdown(6, AttributionScope::kWindow,
                              "https://example.com/iframe3", "example-id6",
                              "https://example.com/iframe3"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(MeasurementToJSON(result), MeasurementToJSON(expected_result));
}

TEST_F(WebMemoryAggregatorTest, FindAggregationStartNode) {
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  FrameNodeImpl* cross_site_child = AddFrameNode(
      "https://foo.com/iframe.html", Bytes{5}, main_frame, "example-id", "");
  FrameNodeImpl* same_site_child =
      AddFrameNode("https://foo.com/iframe2.html", Bytes{4}, cross_site_child,
                   "example-id2", "");

  // FindAggregationStartNode should return the parent foo.com frame for either
  // foo.com child. It should not return the main frame since it's cross-site
  // from the requesting frames.
  EXPECT_EQ(internal::FindAggregationStartNode(cross_site_child),
            cross_site_child);
  EXPECT_EQ(internal::FindAggregationStartNode(same_site_child),
            cross_site_child);

  // When aggregation starts at |cross_site_child| it should not include any
  // memory from the main frame.
  WebMemoryAggregator aggregator(cross_site_child);
  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(5, AttributionScope::kWindow,
                              "https://foo.com/iframe.html"),
      ExpectedMemoryBreakdown(4, AttributionScope::kWindow,
                              "https://foo.com/iframe2.html", "example-id2",
                              ""),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(MeasurementToJSON(result), MeasurementToJSON(expected_result));

  // When the main frame requests a measurement of the same tree it should
  // aggregate the children, which are cross-site from it.
  EXPECT_EQ(internal::FindAggregationStartNode(main_frame), main_frame);
  auto main_frame_expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(9, AttributionScope::kCrossOriginAggregated,
                              base::nullopt, "example-id", ""),
  });
  WebMemoryAggregator main_frame_aggregator(main_frame);
  auto main_frame_result = main_frame_aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(MeasurementToJSON(main_frame_result),
            MeasurementToJSON(main_frame_expected_result));
}

TEST_F(WebMemoryAggregatorTest, AggregateWindowOpener) {
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  FrameNodeImpl* child_frame = AddFrameNode("https://example.com/iframe.html",
                                            Bytes{5}, main_frame, "example-id");

  FrameNodeImpl* opened_frame = AddFrameNodeFromOpener(
      "https://example.com/window/", Bytes{4}, main_frame);
  FrameNodeImpl* child_of_opened_frame =
      AddFrameNode("https://example.com/window-iframe.html", Bytes{3},
                   opened_frame, "example-id2");
  FrameNodeImpl* cross_site_child =
      AddFrameNode("https://cross-site-example.com/window-iframe.html",
                   Bytes{2}, opened_frame, "example-id3");

  // COOP+COEP forces cross-site windows to open in their own BrowsingInstance.
  FrameNodeImpl* cross_site_popup = AddCrossBrowsingInstanceFrameNodeFromOpener(
      "https://cross-site-example.com/", Bytes{2}, main_frame);

  // FindAggregationStartNode whould return |main_frame| from any of the
  // same-site frames.
  for (auto* frame :
       {main_frame, child_frame, opened_frame, child_of_opened_frame}) {
    EXPECT_EQ(internal::FindAggregationStartNode(frame), main_frame)
        << frame->url();
  }

  WebMemoryAggregator aggregator(main_frame);

  // Test the relationships of each node in the graph.
  EXPECT_EQ(aggregator.FindNodeAggregationType(main_frame),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(aggregator.FindNodeAggregationType(child_frame),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                child_frame, aggregator.requesting_origin()),
            main_frame);
  EXPECT_EQ(aggregator.FindNodeAggregationType(opened_frame),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                opened_frame, aggregator.requesting_origin()),
            main_frame);
  EXPECT_EQ(aggregator.FindNodeAggregationType(child_of_opened_frame),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                child_of_opened_frame, aggregator.requesting_origin()),
            opened_frame);
  EXPECT_EQ(aggregator.FindNodeAggregationType(cross_site_child),
            NodeAggregationType::kCrossOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                cross_site_child, aggregator.requesting_origin()),
            opened_frame);
  EXPECT_EQ(aggregator.FindNodeAggregationType(cross_site_popup),
            NodeAggregationType::kInvisible);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                cross_site_popup, aggregator.requesting_origin()),
            main_frame);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(5, AttributionScope::kWindow,
                              "https://example.com/iframe.html", "example-id"),
      ExpectedMemoryBreakdown(4, AttributionScope::kWindow,
                              "https://example.com/window/"),
      ExpectedMemoryBreakdown(3, AttributionScope::kWindow,
                              "https://example.com/window-iframe.html",
                              "example-id2"),
      ExpectedMemoryBreakdown(2, AttributionScope::kCrossOriginAggregated,
                              base::nullopt, "example-id3"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(MeasurementToJSON(result), MeasurementToJSON(expected_result));

  // The two cross-site frames should only be able to see themselves (and their
  // own children, if they had any). They have the same |bytes| so their
  // expectations only vary by url.
  for (auto* frame : {cross_site_child, cross_site_popup}) {
    const std::string url = frame->url().spec();
    SCOPED_TRACE(url);

    const FrameNode* start_node = internal::FindAggregationStartNode(frame);
    EXPECT_EQ(start_node, frame);

    WebMemoryAggregator aggregator(start_node);
    // Only check the NodeAggregationType of the single node that's iterated
    // over. Parents of the start node have an undefined aggregation type.
    EXPECT_EQ(aggregator.FindNodeAggregationType(start_node),
              NodeAggregationType::kSameOriginAggregationPoint);

    auto expected_cross_site_result = CreateExpectedMemoryMeasurement({
        ExpectedMemoryBreakdown(2, AttributionScope::kWindow, url,
                                base::nullopt, base::nullopt),
    });
    auto cross_site_result = aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(MeasurementToJSON(cross_site_result),
              MeasurementToJSON(expected_cross_site_result));
  }
}

}  // namespace v8_memory

}  // namespace performance_manager
