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
#include "components/performance_manager/test_support/performance_manager_test_harness.h"
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
  EXPECT_CHECK_DEATH(TearDownAndDestroyGraph());

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

namespace {

// This class is non-sensically both a GraphRegistered object and a
// PerformanceManagerRegistered object. This is done to ensure that the
// implementations use the appropriately typed "TypeId" functions, as there are
// now two of them available!
class Baz : public GraphRegisteredImpl<Baz>,
            public PerformanceManagerRegisteredImpl<Baz> {
 public:
  Baz() = default;
  ~Baz() override = default;
};

using GraphAndPerformanceManagerRegisteredTest = PerformanceManagerTestHarness;

}  // namespace

TEST_F(GraphAndPerformanceManagerRegisteredTest, GraphAndPMRegistered) {
  // Each TypeId should be backed by a distinct "TypeId" implementation and
  // value.
  const uintptr_t kGraphId = GraphRegisteredImpl<Baz>::TypeId();
  const uintptr_t kPMId = PerformanceManagerRegisteredImpl<Baz>::TypeId();
  ASSERT_NE(kGraphId, kPMId);

  // Create a stand-alone graph that is bound to this sequence so we can test
  // both the PM and a graph on the same sequence.
  std::unique_ptr<GraphImpl> graph(new GraphImpl());
  graph->SetUp();

  PerformanceManagerRegistryImpl* registry =
      PerformanceManagerRegistryImpl::GetInstance();

  Baz baz;
  graph->RegisterObject(&baz);
  registry->RegisterObject(&baz);

  EXPECT_EQ(&baz, graph->GetRegisteredObject(kGraphId));
  EXPECT_EQ(&baz, registry->GetRegisteredObject(kPMId));
  EXPECT_EQ(nullptr, graph->GetRegisteredObject(kPMId));
  EXPECT_EQ(nullptr, registry->GetRegisteredObject(kGraphId));

  // This directly tests that the templated helper function uses the correct
  // instance of TypeId.
  EXPECT_EQ(&baz, graph->GetRegisteredObjectAs<Baz>());
  EXPECT_EQ(&baz, PerformanceManager::GetRegisteredObjectAs<Baz>());

  graph->UnregisterObject(&baz);
  registry->UnregisterObject(&baz);

  graph->TearDown();
  graph.reset();
}

}  // namespace performance_manager