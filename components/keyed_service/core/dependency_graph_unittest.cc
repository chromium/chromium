// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/dependency_graph.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/dependency_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

class DependencyGraphTest : public testing::Test {};

class DummyNode : public DependencyNode {
 public:
  explicit DummyNode(DependencyGraph* graph) : dependency_graph_(graph) {
    dependency_graph_->AddNode(this);
  }

  DummyNode(const DummyNode&) = delete;
  DummyNode& operator=(const DummyNode&) = delete;

  ~DummyNode() { dependency_graph_->RemoveNode(this); }

 private:
  raw_ptr<DependencyGraph> dependency_graph_;
};

// Tests that we can deal with a single component.
TEST_F(DependencyGraphTest, SingleCase) {
  DependencyGraph graph;
  DummyNode node(&graph);

  std::vector<raw_ptr<DependencyNode, VectorExperimental>> construction_order;
  EXPECT_TRUE(graph.GetConstructionOrder(&construction_order));
  ASSERT_EQ(1U, construction_order.size());
  EXPECT_EQ(&node, construction_order[0]);

  std::vector<raw_ptr<DependencyNode, VectorExperimental>> destruction_order;
  EXPECT_TRUE(graph.GetDestructionOrder(&destruction_order));
  ASSERT_EQ(1U, destruction_order.size());
  EXPECT_EQ(&node, destruction_order[0]);
}

// Tests that we get a simple one component depends on the other case.
TEST_F(DependencyGraphTest, SimpleDependency) {
  DependencyGraph graph;
  DummyNode parent(&graph);
  DummyNode child(&graph);

  graph.AddEdge(&parent, &child);

  std::vector<raw_ptr<DependencyNode, VectorExperimental>> construction_order;
  EXPECT_TRUE(graph.GetConstructionOrder(&construction_order));
  ASSERT_EQ(2U, construction_order.size());
  EXPECT_EQ(&parent, construction_order[0]);
  EXPECT_EQ(&child, construction_order[1]);

  std::vector<raw_ptr<DependencyNode, VectorExperimental>> destruction_order;
  EXPECT_TRUE(graph.GetDestructionOrder(&destruction_order));
  ASSERT_EQ(2U, destruction_order.size());
  EXPECT_EQ(&child, destruction_order[0]);
  EXPECT_EQ(&parent, destruction_order[1]);
}

// Tests two children, one parent.
TEST_F(DependencyGraphTest, TwoChildrenOneParent) {
  DependencyGraph graph;
  DummyNode parent(&graph);
  DummyNode child1(&graph);
  DummyNode child2(&graph);

  graph.AddEdge(&parent, &child1);
  graph.AddEdge(&parent, &child2);

  std::vector<raw_ptr<DependencyNode, VectorExperimental>> construction_order;
  EXPECT_TRUE(graph.GetConstructionOrder(&construction_order));
  ASSERT_EQ(3U, construction_order.size());
  EXPECT_EQ(&parent, construction_order[0]);
  EXPECT_EQ(&child1, construction_order[1]);
  EXPECT_EQ(&child2, construction_order[2]);

  std::vector<raw_ptr<DependencyNode, VectorExperimental>> destruction_order;
  EXPECT_TRUE(graph.GetDestructionOrder(&destruction_order));
  ASSERT_EQ(3U, destruction_order.size());
  EXPECT_EQ(&child2, destruction_order[0]);
  EXPECT_EQ(&child1, destruction_order[1]);
  EXPECT_EQ(&parent, destruction_order[2]);
}

// Tests an M configuration.
TEST_F(DependencyGraphTest, MConfiguration) {
  DependencyGraph graph;

  DummyNode parent1(&graph);
  DummyNode parent2(&graph);

  DummyNode child_of_1(&graph);
  graph.AddEdge(&parent1, &child_of_1);

  DummyNode child_of_12(&graph);
  graph.AddEdge(&parent1, &child_of_12);
  graph.AddEdge(&parent2, &child_of_12);

  DummyNode child_of_2(&graph);
  graph.AddEdge(&parent2, &child_of_2);

  std::vector<raw_ptr<DependencyNode, VectorExperimental>> construction_order;
  EXPECT_TRUE(graph.GetConstructionOrder(&construction_order));
  ASSERT_EQ(5U, construction_order.size());
  EXPECT_EQ(&parent1, construction_order[0]);
  EXPECT_EQ(&parent2, construction_order[1]);
  EXPECT_EQ(&child_of_1, construction_order[2]);
  EXPECT_EQ(&child_of_12, construction_order[3]);
  EXPECT_EQ(&child_of_2, construction_order[4]);

  std::vector<raw_ptr<DependencyNode, VectorExperimental>> destruction_order;
  EXPECT_TRUE(graph.GetDestructionOrder(&destruction_order));
  ASSERT_EQ(5U, destruction_order.size());
  EXPECT_EQ(&child_of_2, destruction_order[0]);
  EXPECT_EQ(&child_of_12, destruction_order[1]);
  EXPECT_EQ(&child_of_1, destruction_order[2]);
  EXPECT_EQ(&parent2, destruction_order[3]);
  EXPECT_EQ(&parent1, destruction_order[4]);
}

// Tests that it can deal with a simple diamond.
TEST_F(DependencyGraphTest, DiamondConfiguration) {
  DependencyGraph graph;

  DummyNode parent(&graph);

  DummyNode middle1(&graph);
  graph.AddEdge(&parent, &middle1);

  DummyNode middle2(&graph);
  graph.AddEdge(&parent, &middle2);

  DummyNode bottom(&graph);
  graph.AddEdge(&middle1, &bottom);
  graph.AddEdge(&middle2, &bottom);

  std::vector<raw_ptr<DependencyNode, VectorExperimental>> construction_order;
  EXPECT_TRUE(graph.GetConstructionOrder(&construction_order));
  ASSERT_EQ(4U, construction_order.size());
  EXPECT_EQ(&parent, construction_order[0]);
  EXPECT_EQ(&middle1, construction_order[1]);
  EXPECT_EQ(&middle2, construction_order[2]);
  EXPECT_EQ(&bottom, construction_order[3]);

  std::vector<raw_ptr<DependencyNode, VectorExperimental>> destruction_order;
  EXPECT_TRUE(graph.GetDestructionOrder(&destruction_order));
  ASSERT_EQ(4U, destruction_order.size());
  EXPECT_EQ(&bottom, destruction_order[0]);
  EXPECT_EQ(&middle2, destruction_order[1]);
  EXPECT_EQ(&middle1, destruction_order[2]);
  EXPECT_EQ(&parent, destruction_order[3]);
}

std::string NodeNameProvider(const std::string& name, DependencyNode* node) {
  return name;
}

// When this returns true, then |name| is a valid ID according to the DOT
// language specification [1]. Note that DOT language also allows HTML strings
// as valid identifiers, but those are not used in the production code calling
// the tested DumpAsGraphviz.
// [1] http://www.graphviz.org/content/dot-language
bool IsValidDotId(std::string_view name) {
  static const char pattern[] =
      "[a-zA-Z\\200-\\377_][0-9a-zA-Z\\200-\\377_]*"
      "|-?(?:\\.[0-9]+|[0-9]+(\\.[0-9]*)?)"
      "|\"(?:[^\"]|\\\")*\"";
  return RE2::FullMatch(name, pattern);
}

// Returns the source name of the first edge of the graphstr described in DOT
// format in |graphstr|.
std::string_view LocateNodeNameInGraph(std::string_view graphstr) {
  std::string_view name;
  EXPECT_TRUE(RE2::FullMatch(
      graphstr, "(?sm).*^[ \\t]*([^ \\t]*(?:[ \\t]+[^ \\t]+)*)[ \\t]*->.*",
      &name))
      << "graphstr=" << graphstr;
  return name;
}

// Node names in the dependency graph should be properly escaped.
TEST_F(DependencyGraphTest, DumpAsGraphviz_Escaping) {
  const char* names[] = {
      "namespace::Service",           "justAlphabeticService",
      "Service_with_underscores",     "AlphaNumeric123Service",
      "123ServiceStartingWithDigits", "service with spaces",
      "ServiceWith\"Quotes"};

  DependencyGraph graph;
  DummyNode parent(&graph);
  DummyNode child(&graph);

  graph.AddEdge(&parent, &child);

  for (const char* name : names) {
    SCOPED_TRACE(testing::Message("name=") << name);
    std::string graph_str = graph.DumpAsGraphviz(
        "Test", base::BindRepeating(&NodeNameProvider, name));
    std::string_view dumped_name(LocateNodeNameInGraph(graph_str));
    EXPECT_TRUE(IsValidDotId(dumped_name)) << "dumped_name=" << dumped_name;
  }
}

}  // namespace
