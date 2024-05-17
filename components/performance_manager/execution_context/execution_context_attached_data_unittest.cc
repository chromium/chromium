// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context/execution_context_attached_data.h"

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/execution_context/execution_context_registry_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace execution_context {

namespace {

class FakeData : public ExecutionContextAttachedData<FakeData> {
 public:
  FakeData() = default;
  explicit FakeData(const ExecutionContext* ec) {}
  ~FakeData() override = default;
};

class ExecutionContextAttachedDataTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  ExecutionContextAttachedDataTest() = default;
  ExecutionContextAttachedDataTest(const ExecutionContextAttachedDataTest&) =
      delete;
  ExecutionContextAttachedDataTest& operator=(
      const ExecutionContextAttachedDataTest&) = delete;
  ~ExecutionContextAttachedDataTest() override = default;

  void SetUp() override {
    Super::SetUp();
    registry_ = GraphRegisteredImpl<ExecutionContextRegistryImpl>::GetFromGraph(
        graph());
  }

 protected:
  raw_ptr<ExecutionContextRegistryImpl> registry_ = nullptr;
};

}  // namespace

TEST_F(ExecutionContextAttachedDataTest, AdapterWorks) {
  MockMultiplePagesAndWorkersWithMultipleProcessesGraph mock_graph(graph());

  auto* ec1 =
      registry_->GetExecutionContextForFrameNode(mock_graph.frame.get());
  auto* ec2 =
      registry_->GetExecutionContextForWorkerNode(mock_graph.worker.get());

  EXPECT_FALSE(FakeData::Destroy(ec1));
  FakeData* fd1 = FakeData::Get(ec1);
  EXPECT_FALSE(fd1);
  fd1 = FakeData::GetOrCreate(ec1);
  EXPECT_TRUE(fd1);
  EXPECT_EQ(fd1, FakeData::Get(ec1));
  EXPECT_TRUE(FakeData::Destroy(ec1));
  EXPECT_FALSE(FakeData::Get(ec1));

  EXPECT_FALSE(FakeData::Destroy(ec2));
  FakeData* fd2 = FakeData::Get(ec2);
  EXPECT_FALSE(fd2);
  fd2 = FakeData::GetOrCreate(ec2);
  EXPECT_TRUE(fd2);
  EXPECT_EQ(fd2, FakeData::Get(ec2));
  EXPECT_TRUE(FakeData::Destroy(ec2));
  EXPECT_FALSE(FakeData::Get(ec2));
}

}  // namespace execution_context
}  // namespace performance_manager
