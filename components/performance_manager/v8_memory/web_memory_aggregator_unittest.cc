// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/v8_memory/web_memory_aggregator.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

struct ExpectedMemoryBreakdown {
  WebMemoryTestHarness::Bytes bytes = 0;
  AttributionScope scope = AttributionScope::kWindow;
  std::optional<std::string> url;
  std::optional<std::string> id;
  std::optional<std::string> src;
  WebMemoryTestHarness::Bytes canvas_bytes = 0;

  ExpectedMemoryBreakdown() = default;
  ExpectedMemoryBreakdown(
      WebMemoryTestHarness::Bytes expected_bytes,
      AttributionScope expected_scope,
      std::optional<std::string> expected_url = std::nullopt,
      std::optional<std::string> expected_id = std::nullopt,
      std::optional<std::string> expected_src = std::nullopt,
      WebMemoryTestHarness::Bytes expected_canvas_bytes = std::nullopt)
      : bytes(expected_bytes),
        scope(expected_scope),
        url(std::move(expected_url)),
        id(std::move(expected_id)),
        src(std::move(expected_src)),
        canvas_bytes(expected_canvas_bytes) {}

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
    if (breakdown.canvas_bytes) {
      expected_breakdown->canvas_memory = mojom::WebMemoryUsage::New();
      expected_breakdown->canvas_memory->bytes = breakdown.canvas_bytes.value();
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

// Clone and sort the measurement for easier comparison.
mojom::WebMemoryMeasurementPtr NormalizeMeasurement(
    const mojom::WebMemoryMeasurementPtr& measurement) {
  // Sort all arrays.
  auto canonical_measurement = measurement->Clone();
  for (const auto& breakdown_entry : canonical_measurement->breakdown) {
    std::sort(breakdown_entry->attribution.begin(),
              breakdown_entry->attribution.end());
  }
  std::sort(canonical_measurement->breakdown.begin(),
            canonical_measurement->breakdown.end());

  return canonical_measurement;
}

}  // namespace

class WebMemoryAggregatorTest : public WebMemoryTestHarness {
 protected:
  // Allow individual test subclasses to access private members of
  // WebMemoryAggregator.
  static mojom::WebMemoryBreakdownEntry* CreateBreakdownEntry(
      mojom::WebMemoryAttribution::Scope scope,
      std::optional<std::string> url,
      mojom::WebMemoryMeasurement* measurement) {
    return WebMemoryAggregator::CreateBreakdownEntry(scope, url, measurement);
  }

  static void SetBreakdownAttributionFromFrame(
      const FrameNode* frame_node,
      mojom::WebMemoryBreakdownEntry* breakdown) {
    WebMemoryAggregator::SetBreakdownAttributionFromFrame(frame_node,
                                                          breakdown);
  }

  static void CopyBreakdownAttribution(
      const mojom::WebMemoryBreakdownEntry* from,
      mojom::WebMemoryBreakdownEntry* to) {
    WebMemoryAggregator::CopyBreakdownAttribution(from, to);
  }
};

TEST_F(WebMemoryAggregatorTest, CreateBreakdownEntry) {
  auto measurement = mojom::WebMemoryMeasurement::New();
  auto* breakdown_with_no_url =
      CreateBreakdownEntry(AttributionScope::kCrossOriginAggregated,
                           std::nullopt, measurement.get());
  auto* breakdown_with_url = CreateBreakdownEntry(
      AttributionScope::kWindow, "https://example.com", measurement.get());
  auto* breakdown_with_empty_url =
      CreateBreakdownEntry(AttributionScope::kWindow, "", measurement.get());

  // Ensure breakdowns were added to measurement.
  EXPECT_EQ(measurement->breakdown.size(), 3U);
  EXPECT_EQ(measurement->breakdown[0].get(), breakdown_with_no_url);
  EXPECT_EQ(measurement->breakdown[1].get(), breakdown_with_url);
  EXPECT_EQ(measurement->breakdown[2].get(), breakdown_with_empty_url);

  // Can't use an initializer list because nullopt_t and
  // std::optional<std::string> are different types.
  std::vector<std::optional<std::string>> attributes;
  attributes.push_back(std::nullopt);
  attributes.push_back(std::make_optional("example_attr"));
  attributes.push_back(std::make_optional(""));
  for (const auto& attribute : attributes) {
    SCOPED_TRACE(attribute.value_or("nullopt"));

    // V8ContextTracker needs a parent frame to store attributes.
    FrameNodeImpl* parent_frame =
        attribute ? AddFrameNode("https://example.com", Bytes{1}, nullptr)
                  : nullptr;
    FrameNodeImpl* frame = AddFrameNode("https://example.com", Bytes{1},
                                        parent_frame, attribute, attribute);
    SetBreakdownAttributionFromFrame(frame, breakdown_with_url);
    CopyBreakdownAttribution(breakdown_with_url, breakdown_with_empty_url);

    // All measurements should be created without measurement results.
    auto expected_result = CreateExpectedMemoryMeasurement({
        ExpectedMemoryBreakdown(/*bytes=*/std::nullopt,
                                AttributionScope::kCrossOriginAggregated,
                                /*expected_url=*/std::nullopt,
                                /*expected_id=*/std::nullopt,
                                /*expected_src=*/std::nullopt),
        ExpectedMemoryBreakdown(/*bytes=*/std::nullopt,
                                AttributionScope::kWindow,
                                "https://example.com", attribute, attribute),
        ExpectedMemoryBreakdown(/*bytes=*/std::nullopt,
                                AttributionScope::kWindow,
                                /*expected_url=*/"", attribute, attribute),
    });
    EXPECT_EQ(NormalizeMeasurement(measurement),
              NormalizeMeasurement(expected_result));
  }
}

TEST_F(WebMemoryAggregatorTest, AggregateSingleFrame) {
  // Example 1 from http://wicg.github.io/performance-measure-memory/#examples
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
  });
  WebMemoryAggregator aggregator(main_frame);
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(NormalizeMeasurement(result),
            NormalizeMeasurement(expected_result));
}

TEST_F(WebMemoryAggregatorTest, AggregateSingleSiteMultiFrame) {
  // Example 2 from http://wicg.github.io/performance-measure-memory/#examples
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  AddFrameNode("https://example.com/iframe.html", Bytes{5}, main_frame,
               "example-id", "redirect.html?target=iframe.html");

  WebMemoryAggregator aggregator(main_frame);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(5, AttributionScope::kWindow,
                              "https://example.com/iframe.html", "example-id",
                              "redirect.html?target=iframe.html"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(NormalizeMeasurement(result),
            NormalizeMeasurement(expected_result));
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
  AddFrameNode("https://foo.com/iframe2", Bytes{2}, child_frame, "example-id2",
               "https://foo.com/iframe2");
  AddFrameNode("https://bar.com/iframe2", Bytes{3}, child_frame, "example-id3",
               "https://bar.com/iframe2");

  WorkerNodeImpl* worker =
      AddWorkerNode(WorkerNode::WorkerType::kDedicated,
                    "https://foo.com/worker.js", Bytes{4}, child_frame);

  WebMemoryAggregator aggregator(main_frame);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(14, AttributionScope::kCrossOriginAggregated,
                              std::nullopt, "example-id",
                              "https://foo.com/iframe1"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(NormalizeMeasurement(result),
            NormalizeMeasurement(expected_result));
  worker->RemoveClientFrame(child_frame);
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
  AddFrameNode("https://example.com/iframe2", Bytes{1}, subframe4,
               "example-id5", "https://example.com/iframe2");
  AddFrameNode("https://example.com/iframe3", Bytes{6}, subframe3,
               "example-id6", "https://example.com/iframe3");

  // To test aggregation all the frames above are in the same process, even
  // though in production frames with different origins will be in different
  // processes whenever possible. Frames in a different process from the
  // requesting frame should all have 0 bytes reported.
  AddCrossProcessFrameNode("https://example.com/cross_process", Bytes{100},
                           subframe3, "cross-process-id1");
  AddCrossProcessFrameNode("https://foo.com/cross_process", Bytes{200},
                           subframe3, "cross-process-id2");

  // A frame without a memory measurement (eg. a frame that's added to the frame
  // tree during the measurement) should not have a memory entry in the result.
  AddFrameNode("https://example.com/empty_frame", std::nullopt, subframe3);

  WebMemoryAggregator aggregator(main_frame);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(9, AttributionScope::kCrossOriginAggregated,
                              std::nullopt, "example-id",
                              "https://foo.com/iframe1"),
      ExpectedMemoryBreakdown(3, AttributionScope::kWindow,
                              "https://example.com/iframe1", "example-id",
                              "https://foo.com/iframe1"),
      ExpectedMemoryBreakdown(2, AttributionScope::kCrossOriginAggregated,
                              std::nullopt, "example-id4",
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
                              std::nullopt, "cross-process-id2"),
      ExpectedMemoryBreakdown(std::nullopt, AttributionScope::kWindow,
                              "https://example.com/empty_frame"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(NormalizeMeasurement(result),
            NormalizeMeasurement(expected_result));
}

TEST_F(WebMemoryAggregatorTest, AggregateSameOriginAboutBlank) {
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  AddFrameNode("about:blank", Bytes{20}, main_frame);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(20, AttributionScope::kWindow, "about:blank"),
  });
  WebMemoryAggregator aggregator(main_frame);
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(NormalizeMeasurement(result),
            NormalizeMeasurement(expected_result));
}

TEST_F(WebMemoryAggregatorTest, SkipCrossOriginAboutBlank) {
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  FrameNodeImpl* cross_site_child =
      AddFrameNode("https://foo.com/", Bytes{20}, main_frame);
  AddFrameNode("about:blank", Bytes{30}, cross_site_child);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(50, AttributionScope::kCrossOriginAggregated,
                              std::nullopt),
  });
  WebMemoryAggregator aggregator(main_frame);
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(NormalizeMeasurement(result),
            NormalizeMeasurement(expected_result));
}

TEST_F(WebMemoryAggregatorTest, AggregateWindowOpener) {
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  AddFrameNode("https://example.com/iframe.html", Bytes{5}, main_frame,
               "example-id");

  FrameNodeImpl* opened_frame = AddFrameNodeFromOpener(
      "https://example.com/window/", Bytes{4}, main_frame);
  AddFrameNode("https://example.com/window-iframe.html", Bytes{3}, opened_frame,
               "example-id2");
  FrameNodeImpl* cross_site_child =
      AddFrameNode("https://cross-site-example.com/window-iframe.html",
                   Bytes{2}, opened_frame, "example-id3");

  // COOP+COEP forces cross-site windows to open in their own BrowsingInstance.
  FrameNodeImpl* cross_site_popup = AddCrossBrowsingInstanceFrameNodeFromOpener(
      "https://cross-site-example.com/", Bytes{2}, main_frame);

  WebMemoryAggregator aggregator(main_frame);

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
                              std::nullopt, "example-id3"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(NormalizeMeasurement(result),
            NormalizeMeasurement(expected_result));

  {
    WebMemoryAggregator child_aggregator(cross_site_child);

    auto expected_cross_site_result = CreateExpectedMemoryMeasurement({
        ExpectedMemoryBreakdown(22, AttributionScope::kCrossOriginAggregated),
        ExpectedMemoryBreakdown(
            2, AttributionScope::kWindow,
            "https://cross-site-example.com/window-iframe.html", std::nullopt,
            std::nullopt),
    });
    auto cross_site_result = child_aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(NormalizeMeasurement(cross_site_result),
              NormalizeMeasurement(expected_cross_site_result));
  }

  {
    WebMemoryAggregator popup_aggregator(cross_site_popup);

    auto expected_cross_site_result = CreateExpectedMemoryMeasurement({
        ExpectedMemoryBreakdown(2, AttributionScope::kWindow,
                                "https://cross-site-example.com/", std::nullopt,
                                std::nullopt),
    });
    auto cross_site_result = popup_aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(NormalizeMeasurement(cross_site_result),
              NormalizeMeasurement(expected_cross_site_result));
  }
}

TEST_F(WebMemoryAggregatorTest, AggregateProvisionalWindowOpener) {
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});

  // This creates an openee window with pending navigation which should be
  // skipped because it may get its own browsing context group once the
  // navigation completes.
  AddFrameNodeFromOpener(std::nullopt, Bytes{4}, main_frame);

  WebMemoryAggregator aggregator(main_frame);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(NormalizeMeasurement(result),
            NormalizeMeasurement(expected_result));
}

TEST_F(WebMemoryAggregatorTest, AggregateSameOriginWorker) {
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  FrameNodeImpl* child_frame = AddFrameNode("https://example.com/iframe.html",
                                            Bytes{5}, main_frame, "example-id");
  WorkerNodeImpl* worker1 =
      AddWorkerNode(WorkerNode::WorkerType::kDedicated,
                    "https://example.com/worker1", Bytes{20}, child_frame);
  WorkerNodeImpl* worker2 =
      AddWorkerNode(WorkerNode::WorkerType::kDedicated,
                    "https://example.com/worker2", Bytes{40}, worker1);

  WebMemoryAggregator aggregator(main_frame);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(5, AttributionScope::kWindow,
                              "https://example.com/iframe.html", "example-id"),
      ExpectedMemoryBreakdown(20, AttributionScope::kDedicatedWorker,
                              "https://example.com/worker1", "example-id"),
      ExpectedMemoryBreakdown(40, AttributionScope::kDedicatedWorker,
                              "https://example.com/worker2", "example-id"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(NormalizeMeasurement(result),
            NormalizeMeasurement(expected_result));
  worker2->RemoveClientWorker(worker1);
  worker1->RemoveClientFrame(child_frame);
}

TEST_F(WebMemoryAggregatorTest, AggregateCrossOriginWorker) {
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  FrameNodeImpl* child_frame = AddFrameNode("https://foo.com/iframe.html",
                                            Bytes{5}, main_frame, "example-id");
  WorkerNodeImpl* worker1 =
      AddWorkerNode(WorkerNode::WorkerType::kDedicated,
                    "https://foo.com/worker1", Bytes{20}, child_frame);
  WorkerNodeImpl* worker2 =
      AddWorkerNode(WorkerNode::WorkerType::kDedicated,
                    "https://foo.com/worker2", Bytes{40}, worker1);

  WebMemoryAggregator aggregator(main_frame);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(65, AttributionScope::kCrossOriginAggregated,
                              std::nullopt, "example-id"),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(NormalizeMeasurement(result),
            NormalizeMeasurement(expected_result));
  worker2->RemoveClientWorker(worker1);
  worker1->RemoveClientFrame(child_frame);
}

TEST_F(WebMemoryAggregatorTest, AggregateCrossOriginCallers) {
  FrameNodeImpl* a_com = AddFrameNode("https://a.com/", Bytes{10});
  FrameNodeImpl* a_com_iframe =
      AddFrameNode("https://a.com/iframe", Bytes{20}, a_com, "a_com_iframe");
  FrameNodeImpl* b_com_iframe1 =
      AddFrameNode("https://b.com/iframe1", Bytes{30}, a_com, "b_com_iframe1");
  FrameNodeImpl* b_com_iframe2 = AddFrameNode(
      "https://b.com/iframe2", Bytes{40}, a_com_iframe, "b_com_iframe2");
  FrameNodeImpl* c_com_iframe1 = AddFrameNode(
      "https://c.com/iframe1", Bytes{50}, b_com_iframe1, "c_com_iframe1");
  FrameNodeImpl* a_com_popup1 =
      AddFrameNodeFromOpener("https://a.com/popup1", Bytes{60}, c_com_iframe1);
  FrameNodeImpl* b_com_iframe3 = AddFrameNode(
      "https://b.com/iframe3", Bytes{70}, a_com_popup1, "b_com_iframe3");
  AddFrameNode("https://c.com/iframe2", Bytes{80}, b_com_iframe3,
               "c_com_iframe2");
  AddFrameNodeFromOpener("https://a.com/popup2", Bytes{90}, b_com_iframe2);

  {
    WebMemoryAggregator aggregator(a_com_popup1);
    auto expected_result = CreateExpectedMemoryMeasurement({
        ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                                "https://a.com/"),
        ExpectedMemoryBreakdown(20, AttributionScope::kWindow,
                                "https://a.com/iframe", "a_com_iframe"),
        ExpectedMemoryBreakdown(40, AttributionScope::kCrossOriginAggregated,
                                std::nullopt, "b_com_iframe2"),
        ExpectedMemoryBreakdown(80, AttributionScope::kCrossOriginAggregated,
                                std::nullopt, "b_com_iframe1"),
        ExpectedMemoryBreakdown(60, AttributionScope::kWindow,
                                "https://a.com/popup1"),
        ExpectedMemoryBreakdown(150, AttributionScope::kCrossOriginAggregated,
                                std::nullopt, "b_com_iframe3"),
        ExpectedMemoryBreakdown(90, AttributionScope::kWindow,
                                "https://a.com/popup2"),
    });
    auto result = aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(NormalizeMeasurement(result),
              NormalizeMeasurement(expected_result));
  }

  {
    WebMemoryAggregator aggregator(b_com_iframe3);
    auto expected_result = CreateExpectedMemoryMeasurement({
        ExpectedMemoryBreakdown(180, AttributionScope::kCrossOriginAggregated,
                                std::nullopt),
        ExpectedMemoryBreakdown(40, AttributionScope::kWindow,
                                "https://b.com/iframe2"),
        ExpectedMemoryBreakdown(30, AttributionScope::kWindow,
                                "https://b.com/iframe1"),
        ExpectedMemoryBreakdown(50, AttributionScope::kCrossOriginAggregated,
                                std::nullopt, "c_com_iframe1"),
        ExpectedMemoryBreakdown(70, AttributionScope::kWindow,
                                "https://b.com/iframe3"),
        ExpectedMemoryBreakdown(80, AttributionScope::kCrossOriginAggregated,
                                std::nullopt, "c_com_iframe2"),
    });
    auto result = aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(NormalizeMeasurement(result),
              NormalizeMeasurement(expected_result));
  }

  {
    WebMemoryAggregator aggregator(c_com_iframe1);
    auto expected_result = CreateExpectedMemoryMeasurement({
        ExpectedMemoryBreakdown(320, AttributionScope::kCrossOriginAggregated,
                                std::nullopt),
        ExpectedMemoryBreakdown(50, AttributionScope::kWindow,
                                "https://c.com/iframe1"),
        ExpectedMemoryBreakdown(80, AttributionScope::kWindow,
                                "https://c.com/iframe2"),
    });
    auto result = aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(NormalizeMeasurement(result),
              NormalizeMeasurement(expected_result));
  }
}

TEST_F(WebMemoryAggregatorTest, AggregateCrossProcessCallers) {
  FrameNodeImpl* a_com = AddFrameNode("https://a.com/", Bytes{10});
  FrameNodeImpl* b_com_iframe = AddCrossProcessFrameNode(
      "https://b.com/iframe", Bytes{30}, a_com, "b_com_iframe");
  {
    WebMemoryAggregator aggregator(a_com);
    auto expected_result = CreateExpectedMemoryMeasurement({
        ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                                "https://a.com/"),
        ExpectedMemoryBreakdown(0, AttributionScope::kCrossOriginAggregated,
                                std::nullopt, "b_com_iframe"),
    });
    auto result = aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(NormalizeMeasurement(result),
              NormalizeMeasurement(expected_result));
  }

  {
    WebMemoryAggregator aggregator(b_com_iframe);
    auto expected_result = CreateExpectedMemoryMeasurement({
        ExpectedMemoryBreakdown(0, AttributionScope::kCrossOriginAggregated,
                                std::nullopt),
        ExpectedMemoryBreakdown(30, AttributionScope::kWindow,
                                "https://b.com/iframe"),
    });
    auto result = aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(NormalizeMeasurement(result),
              NormalizeMeasurement(expected_result));
  }
}

TEST_F(WebMemoryAggregatorTest, BlinkMemory) {
  FrameNodeImpl* a_com = AddFrameNode("https://a.com/", Bytes{10});
  SetBlinkMemory(Bytes{1000});
  {
    WebMemoryAggregator aggregator(a_com);
    auto expected_result =
        CreateExpectedMemoryMeasurement({ExpectedMemoryBreakdown(
            10, AttributionScope::kWindow, "https://a.com/")});
    expected_result->blink_memory = mojom::WebMemoryUsage::New();
    expected_result->blink_memory->bytes = 1000;
    expected_result->shared_memory = mojom::WebMemoryUsage::New();
    expected_result->shared_memory->bytes = 0;
    expected_result->detached_memory = mojom::WebMemoryUsage::New();
    expected_result->detached_memory->bytes = 0;
    auto result = aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(NormalizeMeasurement(result),
              NormalizeMeasurement(expected_result));
  }
}

TEST_F(WebMemoryAggregatorTest, BlinkMemoryWithoutFrameBytes) {
  FrameNodeImpl* a_com = AddFrameNode("https://a.com/", std::nullopt);
  SetBlinkMemory(Bytes{1000});
  {
    WebMemoryAggregator aggregator(a_com);
    auto expected_result =
        CreateExpectedMemoryMeasurement({ExpectedMemoryBreakdown(
            std::nullopt, AttributionScope::kWindow, "https://a.com/")});
    expected_result->blink_memory = mojom::WebMemoryUsage::New();
    expected_result->blink_memory->bytes = 1000;
    expected_result->shared_memory = mojom::WebMemoryUsage::New();
    expected_result->shared_memory->bytes = 0;
    expected_result->detached_memory = mojom::WebMemoryUsage::New();
    expected_result->detached_memory->bytes = 0;
    auto result = aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(NormalizeMeasurement(result),
              NormalizeMeasurement(expected_result));
  }
}

TEST_F(WebMemoryAggregatorTest, BlinkMemoryMultipleBrowsingInstances) {
  FrameNodeImpl* a_com = AddFrameNode("https://a.com/", Bytes{10});
  AddCrossBrowsingInstanceFrameNode("https://b.com/", Bytes{30});

  SetBlinkMemory(Bytes{1000});
  {
    WebMemoryAggregator aggregator(a_com);
    auto expected_result =
        CreateExpectedMemoryMeasurement({ExpectedMemoryBreakdown(
            10, AttributionScope::kWindow, "https://a.com/")});
    expected_result->blink_memory = mojom::WebMemoryUsage::New();
    // We know Blink memory for both a.com and b.com because they share
    // the same process. We use V8 memory of a.com to estimate its part
    // of Blink memory: 10 / (10 + 30).
    expected_result->blink_memory->bytes = 1000 * 10 / (10 + 30);
    expected_result->shared_memory = mojom::WebMemoryUsage::New();
    expected_result->shared_memory->bytes = 0;
    expected_result->detached_memory = mojom::WebMemoryUsage::New();
    expected_result->detached_memory->bytes = 0;
    auto result = aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(NormalizeMeasurement(result),
              NormalizeMeasurement(expected_result));
  }
}

TEST_F(WebMemoryAggregatorTest, WorkerWithoutData) {
  FrameNodeImpl* main_frame = AddFrameNode("https://example.com/", Bytes{10});
  WorkerNodeImpl* worker =
      AddWorkerNodeWithoutData(WorkerNode::WorkerType::kDedicated, main_frame);
  WebMemoryAggregator aggregator(main_frame);

  auto expected_result = CreateExpectedMemoryMeasurement({
      ExpectedMemoryBreakdown(10, AttributionScope::kWindow,
                              "https://example.com/"),
      ExpectedMemoryBreakdown(std::nullopt, AttributionScope::kDedicatedWorker,
                              ""),
  });
  auto result = aggregator.AggregateMeasureMemoryResult();
  EXPECT_EQ(NormalizeMeasurement(result),
            NormalizeMeasurement(expected_result));
  worker->RemoveClientFrame(main_frame);
}

TEST_F(WebMemoryAggregatorTest, CanvasMemory) {
  FrameNodeImpl* a_com =
      AddFrameNodeWithCanvasMemory("https://a.com/", Bytes{10}, Bytes{20},
                                   nullptr, std::nullopt, std::nullopt);
  {
    WebMemoryAggregator aggregator(a_com);
    ExpectedMemoryBreakdown expected_breakdown;
    expected_breakdown.bytes = 10;
    expected_breakdown.scope = AttributionScope::kWindow;
    expected_breakdown.url = "https://a.com/";
    expected_breakdown.canvas_bytes = 20;
    auto expected_result =
        CreateExpectedMemoryMeasurement({expected_breakdown});
    auto result = aggregator.AggregateMeasureMemoryResult();
    EXPECT_EQ(NormalizeMeasurement(result),
              NormalizeMeasurement(expected_result));
  }
}

}  // namespace v8_memory

}  // namespace performance_manager
