// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/node_data_describer_util.h"

#include <optional>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/performance_manager/public/graph/node_data_describer.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class TestNodeDataDescriber final : public NodeDataDescriber {
 public:
  base::Value::Dict DescribeFrameNodeData(const FrameNode*) const final {
    return Describe("FrameNode");
  }
  base::Value::Dict DescribePageNodeData(const PageNode*) const final {
    return Describe("PageNode");
  }
  base::Value::Dict DescribeProcessNodeData(
      const ProcessNode* node) const final {
    return Describe("ProcessNode");
  }
  base::Value::Dict DescribeSystemNodeData(const SystemNode*) const final {
    return Describe("SystemNode");
  }
  base::Value::Dict DescribeWorkerNodeData(const WorkerNode*) const final {
    return Describe("WorkerNode");
  }

 private:
  base::Value::Dict Describe(std::string_view node_type) const {
    base::Value::Dict description;
    description.Set("node_type", node_type);
    return description;
  }
};

class NodeDataDescriberUtilTest : public GraphTestHarness {
 protected:
  void ExpectEmptyDict(std::string_view msg, std::string_view json) {
    return ExpectDict(msg, json, true);
  }

  void ExpectNonEmptyDict(std::string_view msg, std::string_view json) {
    return ExpectDict(msg, json, false);
  }

 private:
  void ExpectDict(std::string_view msg,
                  std::string_view json,
                  bool expect_empty_dict) {
    SCOPED_TRACE(::testing::Message() << msg << " " << json);
    std::optional<base::Value> parsed_json = base::JSONReader::Read(json);
    ASSERT_TRUE(parsed_json.has_value());
    ASSERT_TRUE(parsed_json.value().is_dict());
    EXPECT_EQ(parsed_json.value().GetDict().empty(), expect_empty_dict);
  }
};

TEST_F(NodeDataDescriberUtilTest, DumpNodeDescription) {
  MockSinglePageWithFrameAndWorkerInSingleProcessGraph mock_graph(graph());

  // Even though no describers are registered, these should all return
  // dictionaries.
  ExpectNonEmptyDict("Frame", DumpNodeDescription(mock_graph.frame.get()));
  ExpectNonEmptyDict("Page", DumpNodeDescription(mock_graph.page.get()));
  ExpectNonEmptyDict("Process", DumpNodeDescription(mock_graph.process.get()));
  ExpectNonEmptyDict("Worker", DumpNodeDescription(mock_graph.worker.get()));

  // SystemNodeImpl has no default describer.
  ExpectEmptyDict("System", DumpNodeDescription(mock_graph.system.get()));
}

TEST_F(NodeDataDescriberUtilTest, EmptyDumpRegisteredDescribers) {
  MockSinglePageWithFrameAndWorkerInSingleProcessGraph mock_graph(graph());

  // With no describers registered, each call should return an empty
  // dictionary.
  ExpectEmptyDict("Frame", DumpRegisteredDescribers(mock_graph.frame.get()));
  ExpectEmptyDict("Page", DumpRegisteredDescribers(mock_graph.page.get()));
  ExpectEmptyDict("Process",
                  DumpRegisteredDescribers(mock_graph.process.get()));
  ExpectEmptyDict("System", DumpRegisteredDescribers(mock_graph.system.get()));
  ExpectEmptyDict("Worker", DumpRegisteredDescribers(mock_graph.worker.get()));
}

TEST_F(NodeDataDescriberUtilTest, DumpRegisteredDescribers) {
  MockSinglePageWithFrameAndWorkerInSingleProcessGraph mock_graph(graph());
  TestNodeDataDescriber test_describer;
  graph()->GetNodeDataDescriberRegistry()->RegisterDescriber(&test_describer,
                                                             "TestDescriber");

  // The registered describer should be called each time.
  ExpectNonEmptyDict("Frame", DumpRegisteredDescribers(mock_graph.frame.get()));
  ExpectNonEmptyDict("Page", DumpRegisteredDescribers(mock_graph.page.get()));
  ExpectNonEmptyDict("Process",
                     DumpRegisteredDescribers(mock_graph.process.get()));
  ExpectNonEmptyDict("System",
                     DumpRegisteredDescribers(mock_graph.system.get()));
  ExpectNonEmptyDict("Worker",
                     DumpRegisteredDescribers(mock_graph.worker.get()));

  graph()->GetNodeDataDescriberRegistry()->UnregisterDescriber(&test_describer);
}

}  // namespace

}  // namespace performance_manager
