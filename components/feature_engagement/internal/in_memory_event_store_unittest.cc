// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/in_memory_event_store.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

class InMemoryEventStoreTest : public ::testing::Test {
 public:
  InMemoryEventStoreTest()
      : load_callback_has_been_invoked_(false), last_result_(false) {}

  void LoadCallback(bool success, std::unique_ptr<std::vector<Event>> events) {
    load_callback_has_been_invoked_ = true;
    last_result_ = success;
    loaded_events_ = std::move(events);
  }

 protected:
  bool load_callback_has_been_invoked_;
  bool last_result_;
  std::unique_ptr<std::vector<Event>> loaded_events_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};
}  // namespace

TEST_F(InMemoryEventStoreTest, LoadShouldProvideEventsAsCallback) {
  std::unique_ptr<std::vector<Event>> events =
      std::make_unique<std::vector<Event>>();
  Event foo;
  Event bar;
  events->push_back(foo);
  events->push_back(bar);

  // Create a new store and verify it's not ready yet.
  InMemoryEventStore store(std::move(events));
  EXPECT_FALSE(store.IsReady());

  // Load the data and ensure the callback is not immediately invoked, since the
  // result should be posted.
  store.Load(base::BindOnce(&InMemoryEventStoreTest::LoadCallback,
                            base::Unretained(this)));
  EXPECT_FALSE(load_callback_has_been_invoked_);

  // Run the message loop until it's idle to finish to ensure the result is
  // available.
  base::RunLoop().RunUntilIdle();

  // The two events should have been loaded, and the store should be ready.
  EXPECT_TRUE(load_callback_has_been_invoked_);
  EXPECT_TRUE(store.IsReady());
  EXPECT_EQ(2u, loaded_events_->size());
  EXPECT_TRUE(last_result_);
}

}  // namespace feature_engagement
