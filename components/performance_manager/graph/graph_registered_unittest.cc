// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/graph/graph_registered.h"

#include <memory>
#include <utility>

#include "base/test/gtest_util.h"
#include "components/performance_manager/performance_manager_registry_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

using GraphRegisteredTest = GraphTestHarness;

class Foo : public GraphRegisteredImpl<Foo> {
 public:
  Foo() = default;
  ~Foo() override = default;
};

class Bar : public GraphRegisteredImpl<Bar> {
 public:
  Bar() = default;
  ~Bar() override = default;
};

// A GraphOwnedAndRegistered that doesn't override OnPassedToGraph and
// OnTakenFromGraph.
class OwnedFoo final : public GraphOwnedAndRegistered<OwnedFoo> {
 public:
  OwnedFoo() = default;
  ~OwnedFoo() final = default;
};

// A GraphOwnedAndRegistered that overrides OnPassedToGraph and
// OnTakenFromGraph.
class OwnedBar final : public GraphOwnedAndRegistered<OwnedBar> {
 public:
  OwnedBar() = default;
  ~OwnedBar() final = default;

  void OnPassedToGraph(Graph* graph) final { on_passed_called_ = true; }
  void OnTakenFromGraph(Graph* graph) final { on_taken_called_ = true; }

  bool on_passed_called() const { return on_passed_called_; }
  bool on_taken_called() const { return on_taken_called_; }

 private:
  bool on_passed_called_ = false;
  bool on_taken_called_ = false;
};

TEST_F(GraphRegisteredTest, GraphRegistrationWorks) {
  // This ensures that the templated distinct TypeId generation works.
  ASSERT_NE(Foo::TypeId(), Bar::TypeId());

  EXPECT_FALSE(graph()->GetRegisteredObject(Foo::TypeId()));
  EXPECT_FALSE(graph()->GetRegisteredObjectAs<Foo>());
  EXPECT_FALSE(graph()->GetRegisteredObject(Bar::TypeId()));
  EXPECT_FALSE(graph()->GetRegisteredObjectAs<Bar>());

  // Insertion works.
  Foo foo;
  graph()->RegisterObject(&foo);
  EXPECT_EQ(&foo, graph()->GetRegisteredObject(Foo::TypeId()));
  EXPECT_EQ(&foo, graph()->GetRegisteredObjectAs<Foo>());
  EXPECT_FALSE(graph()->GetRegisteredObject(Bar::TypeId()));
  EXPECT_FALSE(graph()->GetRegisteredObjectAs<Bar>());

  // Inserting again fails.
  EXPECT_CHECK_DEATH(graph()->RegisterObject(&foo));

  // Unregistered the wrong object fails.
  Foo foo2;
  EXPECT_CHECK_DEATH(graph()->UnregisterObject(&foo2));

  // Unregistering works.
  graph()->UnregisterObject(&foo);
  EXPECT_FALSE(graph()->GetRegisteredObject(Foo::TypeId()));
  EXPECT_FALSE(graph()->GetRegisteredObjectAs<Foo>());
  EXPECT_FALSE(graph()->GetRegisteredObject(Bar::TypeId()));
  EXPECT_FALSE(graph()->GetRegisteredObjectAs<Bar>());

  // Unregistering again fails.
  EXPECT_CHECK_DEATH(graph()->UnregisterObject(&foo));
  EXPECT_CHECK_DEATH(graph()->UnregisterObject(&foo2));

  // Registering multiple objects works.
  Bar bar;
  graph()->RegisterObject(&foo);
  graph()->RegisterObject(&bar);
  EXPECT_EQ(&foo, graph()->GetRegisteredObject(Foo::TypeId()));
  EXPECT_EQ(&foo, graph()->GetRegisteredObjectAs<Foo>());
  EXPECT_EQ(&bar, graph()->GetRegisteredObject(Bar::TypeId()));
  EXPECT_EQ(&bar, graph()->GetRegisteredObjectAs<Bar>());

  // Check the various helper functions.
  EXPECT_EQ(&foo, Foo::GetFromGraph(graph()));
  EXPECT_EQ(&bar, Bar::GetFromGraph(graph()));
  EXPECT_TRUE(foo.IsRegistered(graph()));
  EXPECT_TRUE(bar.IsRegistered(graph()));
  EXPECT_FALSE(Foo::NothingRegistered(graph()));
  EXPECT_FALSE(Bar::NothingRegistered(graph()));
  graph()->UnregisterObject(&bar);
  EXPECT_EQ(&foo, Foo::GetFromGraph(graph()));
  EXPECT_FALSE(Bar::GetFromGraph(graph()));
  EXPECT_TRUE(foo.IsRegistered(graph()));
  EXPECT_FALSE(bar.IsRegistered(graph()));
  EXPECT_FALSE(Foo::NothingRegistered(graph()));
  EXPECT_TRUE(Bar::NothingRegistered(graph()));

  // At this point if the graph is torn down it should explode because foo
  // hasn't been unregistered.
  // TODO(pbos): Figure out why the DCHECK build dies in a different place (a
  // DCHECK) and see if these can be consolidated into one EXPECT_DCHECK_DEATH.
  if (DCHECK_IS_ON()) {
    EXPECT_DCHECK_DEATH(TearDownAndDestroyGraph());
  } else {
    EXPECT_CHECK_DEATH(TearDownAndDestroyGraph());
  }

  graph()->UnregisterObject(&foo);
}

TEST_F(GraphRegisteredTest, GraphOwnedAndRegistered) {
  // Insertion works.
  EXPECT_FALSE(graph()->GetRegisteredObjectAs<OwnedFoo>());
  auto unique_foo = std::make_unique<OwnedFoo>();
  EXPECT_EQ(unique_foo->GetOwningGraph(), nullptr);
  OwnedFoo* foo = graph()->PassToGraph(std::move(unique_foo));
  EXPECT_EQ(foo, graph()->GetRegisteredObjectAs<OwnedFoo>());
  EXPECT_EQ(foo->GetOwningGraph(), graph());

  EXPECT_FALSE(graph()->GetRegisteredObjectAs<OwnedBar>());
  auto unique_bar = std::make_unique<OwnedBar>();
  EXPECT_EQ(unique_bar->GetOwningGraph(), nullptr);
  OwnedBar* bar = graph()->PassToGraph(std::move(unique_bar));
  EXPECT_EQ(bar, graph()->GetRegisteredObjectAs<OwnedBar>());
  EXPECT_TRUE(bar->on_passed_called());
  EXPECT_EQ(bar->GetOwningGraph(), graph());

  // Inserting again fails.
  EXPECT_CHECK_DEATH(graph()->RegisterObject(foo));
  EXPECT_CHECK_DEATH(graph()->PassToGraph(std::make_unique<OwnedFoo>()));

  // Unregistering works.
  std::unique_ptr<OwnedFoo> unique_foo2 =
      graph()->TakeFromGraphAs<OwnedFoo>(foo);
  EXPECT_EQ(foo, unique_foo2.get());
  EXPECT_EQ(nullptr, graph()->GetRegisteredObjectAs<OwnedFoo>());
  EXPECT_EQ(foo->GetOwningGraph(), nullptr);

  std::unique_ptr<OwnedBar> unique_bar2 =
      graph()->TakeFromGraphAs<OwnedBar>(bar);
  EXPECT_EQ(bar, unique_bar2.get());
  EXPECT_EQ(nullptr, graph()->GetRegisteredObjectAs<OwnedBar>());
  EXPECT_TRUE(bar->on_taken_called());
  EXPECT_EQ(bar->GetOwningGraph(), nullptr);

  // Unregistering again fails.
  EXPECT_CHECK_DEATH(graph()->UnregisterObject(foo));

  // Passing back an object that was taken should re-register it.
  graph()->PassToGraph(std::move(unique_foo2));
  EXPECT_EQ(foo, graph()->GetRegisteredObjectAs<OwnedFoo>());
  EXPECT_EQ(foo->GetOwningGraph(), graph());

  // At this point the graph can be safely torn down because
  // GraphOwnedAndRegistered objects will be deleted and unregistered.
  TearDownAndDestroyGraph();
}

TEST_F(GraphRegisteredTest, GetFromGraph_Default) {
  PerformanceManagerTestHarnessHelper pm_helper;

  // Before PerformanceManager is available, GetFromGraph() should safely return
  // nullptr.
  EXPECT_FALSE(PerformanceManager::IsAvailable());
  EXPECT_EQ(Foo::GetFromGraph(), nullptr);
  EXPECT_EQ(OwnedFoo::GetFromGraph(), nullptr);

  pm_helper.SetUp();

  // Objects not registered.
  EXPECT_TRUE(PerformanceManager::IsAvailable());
  EXPECT_EQ(Foo::GetFromGraph(), nullptr);
  EXPECT_EQ(OwnedFoo::GetFromGraph(), nullptr);

  // Register objects.
  Graph* graph = PerformanceManager::GetGraph();
  ASSERT_TRUE(graph);
  Foo foo;
  graph->RegisterObject(&foo);
  OwnedFoo* owned_foo = graph->PassToGraph(std::make_unique<OwnedFoo>());
  EXPECT_EQ(Foo::GetFromGraph(), &foo);
  EXPECT_EQ(OwnedFoo::GetFromGraph(), owned_foo);

  // Remove objects.
  graph->UnregisterObject(&foo);
  graph->TakeFromGraph(owned_foo);
  EXPECT_EQ(Foo::GetFromGraph(), nullptr);
  EXPECT_EQ(OwnedFoo::GetFromGraph(), nullptr);

  pm_helper.TearDown();

  // After PerformanceManager is gone, GetFromGraph() should safely return
  // nullptr again.
  EXPECT_FALSE(PerformanceManager::IsAvailable());
  EXPECT_EQ(Foo::GetFromGraph(), nullptr);
  EXPECT_EQ(OwnedFoo::GetFromGraph(), nullptr);
}

TEST_F(GraphRegisteredTest, GetFromGraph_WithParam) {
  PerformanceManagerTestHarnessHelper pm_helper;
  pm_helper.SetUp();

  Graph* graph = PerformanceManager::GetGraph();
  TestGraphImpl graph2;
  graph2.SetUp();

  OwnedFoo* foo = graph->PassToGraph(std::make_unique<OwnedFoo>());
  OwnedBar* bar = graph2.PassToGraph(std::make_unique<OwnedBar>());

  EXPECT_EQ(OwnedFoo::GetFromGraph(), foo);
  EXPECT_EQ(OwnedFoo::GetFromGraph(graph), foo);
  EXPECT_EQ(OwnedFoo::GetFromGraph(&graph2), nullptr);

  EXPECT_EQ(OwnedBar::GetFromGraph(), nullptr);
  EXPECT_EQ(OwnedBar::GetFromGraph(graph), nullptr);
  EXPECT_EQ(OwnedBar::GetFromGraph(&graph2), bar);

  // Passing a parameter should still work when PM is not available.
  pm_helper.TearDown();
  EXPECT_FALSE(PerformanceManager::IsAvailable());
  EXPECT_EQ(OwnedBar::GetFromGraph(&graph2), bar);

  graph2.TearDown();
}

}  // namespace performance_manager
