// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/broker_init.h"

#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

TEST(BrokerInitTest,
     QueuesTasksUntilCompleteAndUpdatesCapabilitiesOnSubsequentComplete) {
  BrokerInit init;
  EXPECT_FALSE(init.is_complete());

  base::test::TestFuture<const on_device_model::Capabilities&> first_task;
  base::test::TestFuture<const on_device_model::Capabilities&> second_task;
  init.QueueTask(first_task.GetCallback());
  init.QueueTask(second_task.GetCallback());
  EXPECT_FALSE(init.is_complete());
  EXPECT_FALSE(first_task.IsReady());
  EXPECT_FALSE(second_task.IsReady());

  const on_device_model::Capabilities initial_capabilities{};
  init.Complete(initial_capabilities);
  EXPECT_TRUE(init.is_complete());
  EXPECT_EQ(first_task.Get(), initial_capabilities);
  EXPECT_EQ(second_task.Get(), initial_capabilities);

  // Subsequent Complete() calls update capabilities_ for future tasks without
  // re-running already completed tasks.
  const on_device_model::Capabilities updated_capabilities{
      on_device_model::CapabilityFlags::kAudioInput};
  init.Complete(updated_capabilities);

  base::test::TestFuture<const on_device_model::Capabilities&> post_init_task;
  init.QueueTask(post_init_task.GetCallback());
  EXPECT_TRUE(post_init_task.IsReady());
  EXPECT_EQ(post_init_task.Get(), updated_capabilities);
}

}  // namespace
}  // namespace optimization_guide
