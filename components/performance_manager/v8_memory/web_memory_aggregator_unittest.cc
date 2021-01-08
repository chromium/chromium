// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_aggregator.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/trace_event/traced_value.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/v8_memory/web_memory.h"
#include "components/performance_manager/v8_memory/v8_memory_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager {

namespace v8_memory {

namespace {

using AttributionScope = mojom::WebMemoryAttribution::Scope;
using NodeAggregationType = WebMemoryAggregator::NodeAggregationType;

using WebMemoryAggregatorTest = WebMemoryTestHarness;

struct ExpectedMemoryBreakdown {
  WebMemoryTestHarness::Bytes bytes;
  AttributionScope scope = AttributionScope::kWindow;
  base::Optional<std::string> url;
  base::Optional<std::string> id;
  base::Optional<std::string> src;

  ExpectedMemoryBreakdown() = default;
  ExpectedMemoryBreakdown(
      WebMemoryTestHarness::Bytes expected_bytes,
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
    if (breakdown.bytes) {
      expected_breakdown->memory = mojom::WebMemoryUsage::New();
      expected_breakdown->memory->bytes = breakdown.bytes.value();
    }

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

    // All measurements should be created without measurement results.
    auto expected_result = CreateExpectedMemoryMeasurement({
        ExpectedMemoryBreakdown(/*bytes=*/base::nullopt,
                                AttributionScope::kCrossOriginAggregated,
                                /*expected_url=*/base::nullopt,
                                /*expected_id=*/base::nullopt,
                                /*expected_src=*/base::nullopt),
        ExpectedMemoryBreakdown(/*bytes=*/base::nullopt,
                                AttributionScope::kWindow,
                                "https://example.com", attribute, attribute),
        ExpectedMemoryBreakdown(/*bytes=*/base::nullopt,
                                AttributionScope::kWindow,
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

  // To test aggregation all the frames above are in the same process, even
  // though in production frames with different origins will be in different
  // processes whenever possible. Frames in a different process from the
  // requesting frame should all have 0 bytes reported.
  FrameNodeImpl* cross_process_frame =
      AddCrossProcessFrameNode("https://example.com/cross_process", Bytes{100},
                               subframe3, "cross-process-id1");
  FrameNodeImpl* cross_process_frame2 =
      AddCrossProcessFrameNode("https://foo.com/cross_process", Bytes{200},
                               subframe3, "cross-process-id2");

  // A frame without a memory measurement (eg. a frame that's added to the frame
  // tree during the measurement) should not have a memory entry in the result.
  FrameNodeImpl* empty_frame =
      AddFrameNode("https://example.com/empty_frame", base::nullopt, subframe3);

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
  EXPECT_EQ(aggregator.FindNodeAggregationType(cross_process_frame),
            NodeAggregationType::kSameOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                cross_process_frame, aggregator.requesting_origin()),
            subframe3);
  EXPECT_EQ(aggregator.FindNodeAggregationType(cross_process_frame2),
            NodeAggregationType::kCrossOriginAggregationPoint);
  EXPECT_EQ(internal::GetSameOriginParentOrOpener(
                cross_process_frame2, aggregator.requesting_origin()),
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
      ExpectedMemoryBreakdown(0, AttributionScope::kWindow,
                              "https://example.com/cross_process",
                              "cross-process-id1"),
      ExpectedMemoryBreakdown(0, AttributionScope::kCrossOriginAggregated,
                              base::nullopt, "cross-process-id2"),
      ExpectedMemoryBreakdown(base::nullopt, AttributionScope::kWindow,
                              "https://example.com/empty_frame"),
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

TEST_F(WebMemoryAggregatorTest, FindCrossProcessAggregationStartNode) {
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{1});
  FrameNodeImpl* cross_process_child = AddCrossProcessFrameNode(
      "https://example.com/cross_process.html", Bytes{2}, main_frame);
  FrameNodeImpl* same_process_child = AddFrameNode(
      "https://example.com/same_process.html", Bytes{3}, cross_process_child);

  auto origin = url::Origin::Create(GURL("https://example.com"));
  ASSERT_EQ(internal::GetSameOriginParentOrOpener(cross_process_child, origin),
            main_frame);
  ASSERT_EQ(internal::GetSameOriginParentOrOpener(same_process_child, origin),
            cross_process_child);

  // |cross_process_child| has no ancestor in the same process as it.
  EXPECT_EQ(internal::FindAggregationStartNode(cross_process_child),
            cross_process_child);

  // The search starting from |same_process_child| should skip over
  // |cross_process_child|, which is in a different process, and find
  // |main_frame| which is in the same process.
  EXPECT_EQ(internal::FindAggregationStartNode(same_process_child), main_frame);
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

TEST_F(WebMemoryAggregatorTest, AggregateProvisionalWindowOpener) {
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});

  // This creates an openee window with pending navigation which should be
  // skipped because it may get its own browsing context group once the
  // navigation completes.
  FrameNodeImpl* pending_frame =
      AddFrameNodeFromOpener(base::nullopt, Bytes{4}, main_frame);

  WebMemoryAggregator aggregator(main_frame);

  EXPECT_EQ(aggregator.FindNodeAggregationType(pending_frame),
            NodeAggregationType::kInvisible);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(MeasurementToJSON(result), MeasurementToJSON(expected_result));
}

}  // namespace

}  // namespace v8_memory

}  // namespace performance_manager
