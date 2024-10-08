// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/event_model_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "components/feature_engagement/internal/editable_configuration.h"
#include "components/feature_engagement/internal/in_memory_event_store.h"
#include "components/feature_engagement/internal/never_event_storage_validator.h"
#include "components/feature_engagement/internal/proto/feature_event.pb.h"
#include "components/feature_engagement/internal/test/event_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

// A test-only implementation of InMemoryEventStore that tracks calls to
// WriteEvent(...).
class TestInMemoryEventStore : public InMemoryEventStore {
 public:
  TestInMemoryEventStore(std::unique_ptr<std::vector<Event>> events,
                         bool load_should_succeed)
      : InMemoryEventStore(std::move(events)),
        store_operation_count_(0),
        load_should_succeed_(load_should_succeed) {}

  void Load(OnLoadedCallback callback) override {
    HandleLoadResult(std::move(callback), load_should_succeed_);
  }

  void WriteEvent(const Event& event) override {
    ++store_operation_count_;
    last_written_event_ = std::make_unique<Event>(event);
  }

  void DeleteEvent(const std::string& event_name) override {
    ++store_operation_count_;
    last_deleted_event_ = event_name;
  }

  const Event* GetLastWrittenEvent() { return last_written_event_.get(); }

  const std::string GetLastDeletedEvent() { return last_deleted_event_; }

  uint32_t GetStoreOperationCount() { return store_operation_count_; }

 private:
  // Temporary store the last written event.
  std::unique_ptr<Event> last_written_event_;

  // Temporary store the last deleted event.
  std::string last_deleted_event_;

  // Tracks the number of operations performed on the store.
  uint32_t store_operation_count_;

  // Denotes whether the call to Load(...) should succeed or not. This impacts
  // both the ready-state and the result for the OnLoadedCallback.
  bool load_should_succeed_;
};

class TestEventStorageValidator : public EventStorageValidator {
 public:
  TestEventStorageValidator() : should_store_(true) {}

  TestEventStorageValidator(const TestEventStorageValidator&) = delete;
  TestEventStorageValidator& operator=(const TestEventStorageValidator&) =
      delete;

  bool ShouldStore(const std::string& event_name) const override {
    return should_store_;
  }

  bool ShouldKeep(const std::string& event_name,
                  uint32_t event_day,
                  uint32_t current_day) const override {
    auto search = max_keep_ages_.find(event_name);
    if (search == max_keep_ages_.end())
      return false;

    return (current_day - event_day) < search->second;
  }

  void SetShouldStore(bool should_store) { should_store_ = should_store; }

  void SetMaxKeepAge(const std::string& event_name, uint32_t age) {
    max_keep_ages_[event_name] = age;
  }

 private:
  bool should_store_;
  std::map<std::string, uint32_t> max_keep_ages_;
};

// Creates a TestInMemoryEventStore containing three hard coded events.
std::unique_ptr<TestInMemoryEventStore> CreatePrefilledStore() {
  std::unique_ptr<std::vector<Event>> events =
      std::make_unique<std::vector<Event>>();

  Event foo;
  foo.set_name("foo");
  test::SetEventCountForDay(&foo, 1, 1);
  events->push_back(foo);

  Event bar;
  bar.set_name("bar");
  test::SetEventCountForDay(&bar, 1, 3);
  test::SetEventCountForDay(&bar, 2, 3);
  test::SetEventCountForDay(&bar, 5, 5);
  events->push_back(bar);

  Event qux;
  qux.set_name("qux");
  test::SetEventCountForDay(&qux, 1, 5);
  test::SetEventCountForDay(&qux, 2, 1);
  test::SetEventCountForDay(&qux, 3, 2);
  events->push_back(qux);

  return std::make_unique<TestInMemoryEventStore>(std::move(events), true);
}

class EventModelImplTest : public ::testing::Test {
 public:
  EventModelImplTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        handle_(task_runner_),
        got_initialize_callback_(false),
        initialize_callback_result_(false) {}

  void SetUp() override {
    std::unique_ptr<TestInMemoryEventStore> store = CreateStore();
    store_ = store.get();

    auto storage_validator = std::make_unique<TestEventStorageValidator>();
    storage_validator_ = storage_validator.get();

    model_ = std::make_unique<EventModelImpl>(std::move(store),
                                              std::move(storage_validator));

    // By default store all events for a very long time.
    storage_validator_->SetMaxKeepAge("foo", 10000u);
    storage_validator_->SetMaxKeepAge("bar", 10000u);
    storage_validator_->SetMaxKeepAge("qux", 10000u);
  }

  virtual std::unique_ptr<TestInMemoryEventStore> CreateStore() {
    return CreatePrefilledStore();
  }

  void OnModelInitializationFinished(bool success) {
    got_initialize_callback_ = true;
    initialize_callback_result_ = success;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle handle_;

  std::unique_ptr<EventModelImpl> model_;
  raw_ptr<TestInMemoryEventStore> store_;
  raw_ptr<TestEventStorageValidator> storage_validator_;
  bool got_initialize_callback_;
  bool initialize_callback_result_;
};

class LoadFailingEventModelImplTest : public EventModelImplTest {
 public:
  LoadFailingEventModelImplTest() = default;

  std::unique_ptr<TestInMemoryEventStore> CreateStore() override {
    return std::make_unique<TestInMemoryEventStore>(
        std::make_unique<std::vector<Event>>(), false);
  }
};

}  // namespace

TEST_F(EventModelImplTest, InitializeShouldBeReadyImmediatelyAfterCallback) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);

  // Only run pending tasks on the queue.  Do not run any subsequently queued
  // tasks that result from running the current pending tasks.
  task_runner_->RunPendingTasks();

  EXPECT_TRUE(got_initialize_callback_);
  EXPECT_TRUE(model_->IsReady());
}

TEST_F(EventModelImplTest, InitializeShouldLoadEntries) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());
  EXPECT_TRUE(got_initialize_callback_);
  EXPECT_TRUE(initialize_callback_result_);

  // Verify that all the data matches what was put into the store in
  // CreateStore().
  const Event* foo_event = model_->GetEvent("foo");
  EXPECT_EQ("foo", foo_event->name());
  EXPECT_EQ(1, foo_event->events_size());
  test::VerifyEventCount(foo_event, 1u, 1u);

  const Event* bar_event = model_->GetEvent("bar");
  EXPECT_EQ("bar", bar_event->name());
  EXPECT_EQ(3, bar_event->events_size());
  test::VerifyEventCount(bar_event, 1u, 3u);
  test::VerifyEventCount(bar_event, 2u, 3u);
  test::VerifyEventCount(bar_event, 5u, 5u);

  const Event* qux_event = model_->GetEvent("qux");
  EXPECT_EQ("qux", qux_event->name());
  EXPECT_EQ(3, qux_event->events_size());
  test::VerifyEventCount(qux_event, 1u, 5u);
  test::VerifyEventCount(qux_event, 2u, 1u);
  test::VerifyEventCount(qux_event, 3u, 2u);
}

TEST_F(EventModelImplTest, InitializeShouldOnlyLoadEntriesThatShouldBeKept) {
  // Back to day 5, i.e. no entries.
  storage_validator_->SetMaxKeepAge("foo", 1u);

  // Back to day 2, i.e. 2 events.
  storage_validator_->SetMaxKeepAge("bar", 4u);

  // Back to day epoch, i.e. all events.
  storage_validator_->SetMaxKeepAge("qux", 10u);

  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      5u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());
  EXPECT_TRUE(got_initialize_callback_);
  EXPECT_TRUE(initialize_callback_result_);

  // Verify that all the data matches what was put into the store in
  // CreateStore(), minus the events that should no longer exist.
  const Event* foo_event = model_->GetEvent("foo");
  EXPECT_EQ(nullptr, foo_event);
  EXPECT_EQ("foo", store_->GetLastDeletedEvent());

  const Event* bar_event = model_->GetEvent("bar");
  EXPECT_EQ("bar", bar_event->name());
  EXPECT_EQ(2, bar_event->events_size());
  test::VerifyEventCount(bar_event, 2u, 3u);
  test::VerifyEventCount(bar_event, 5u, 5u);
  test::VerifyEventsEqual(bar_event, store_->GetLastWrittenEvent());

  // Nothing has changed for 'qux', so nothing will be written to EventStore.
  const Event* qux_event = model_->GetEvent("qux");
  EXPECT_EQ("qux", qux_event->name());
  EXPECT_EQ(3, qux_event->events_size());
  test::VerifyEventCount(qux_event, 1u, 5u);
  test::VerifyEventCount(qux_event, 2u, 1u);
  test::VerifyEventCount(qux_event, 3u, 2u);

  // In total, only two operations should have happened, the update of "bar",
  // and the delete of "foo".
  EXPECT_EQ(2u, store_->GetStoreOperationCount());
}

TEST_F(EventModelImplTest, RetrievingNewEventsShouldYieldNullptr) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  const Event* no_event = model_->GetEvent("no");
  EXPECT_EQ(nullptr, no_event);
  test::VerifyEventsEqual(nullptr, store_->GetLastWrittenEvent());
}

TEST_F(EventModelImplTest, IncrementingNonExistingEvent) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // Incrementing the event should work even if it does not exist.
  model_->IncrementEvent("nonexisting", 1u);
  const Event* event1 = model_->GetEvent("nonexisting");
  ASSERT_NE(nullptr, event1);
  EXPECT_EQ("nonexisting", event1->name());
  EXPECT_EQ(1, event1->events_size());
  test::VerifyEventCount(event1, 1u, 1u);
  test::VerifyEventsEqual(event1, store_->GetLastWrittenEvent());

  // Incrementing the event after it has been initialized to 1, it should now
  // have a count of 2 for the given day.
  model_->IncrementEvent("nonexisting", 1u);
  const Event* event2 = model_->GetEvent("nonexisting");
  ASSERT_NE(nullptr, event2);
  Event_Count event2_count = event2->events(0);
  EXPECT_EQ(1, event2->events_size());
  test::VerifyEventCount(event2, 1u, 2u);
  test::VerifyEventsEqual(event2, store_->GetLastWrittenEvent());
}

TEST_F(EventModelImplTest, IncrementingNonExistingEventMultipleDays) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  model_->IncrementEvent("nonexisting", 1u);
  model_->IncrementEvent("nonexisting", 2u);
  model_->IncrementEvent("nonexisting", 2u);
  model_->IncrementEvent("nonexisting", 3u);
  const Event* event = model_->GetEvent("nonexisting");
  ASSERT_NE(nullptr, event);
  EXPECT_EQ(3, event->events_size());
  test::VerifyEventCount(event, 1u, 1u);
  test::VerifyEventCount(event, 2u, 2u);
  test::VerifyEventCount(event, 3u, 1u);
  test::VerifyEventsEqual(event, store_->GetLastWrittenEvent());
}

TEST_F(EventModelImplTest, IncrementingNonExistingEventWithoutStoring) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  storage_validator_->SetShouldStore(false);

  // Incrementing the event should not be written or stored in-memory.
  model_->IncrementEvent("nonexisting", 1u);
  const Event* event1 = model_->GetEvent("nonexisting");
  EXPECT_EQ(nullptr, event1);
  test::VerifyEventsEqual(nullptr, store_->GetLastWrittenEvent());
}

TEST_F(EventModelImplTest, IncrementingExistingEventWithoutStoring) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // Write one event before turning off storage.
  model_->IncrementEvent("nonexisting", 1u);
  const Event* first_event = model_->GetEvent("nonexisting");
  ASSERT_NE(nullptr, first_event);
  test::VerifyEventsEqual(first_event, store_->GetLastWrittenEvent());

  storage_validator_->SetShouldStore(false);

  // Incrementing the event should no longer be written or stored in-memory.
  model_->IncrementEvent("nonexisting", 1u);
  const Event* second_event = model_->GetEvent("nonexisting");
  EXPECT_EQ(first_event, second_event);
  test::VerifyEventsEqual(first_event, store_->GetLastWrittenEvent());
}

TEST_F(EventModelImplTest, IncrementingSingleDayExistingEvent) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // |foo| is inserted into the store with a count of 1 at day 1.
  const Event* foo_event = model_->GetEvent("foo");
  EXPECT_EQ("foo", foo_event->name());
  EXPECT_EQ(1, foo_event->events_size());
  test::VerifyEventCount(foo_event, 1u, 1u);

  // Incrementing |foo| should change count to 2.
  model_->IncrementEvent("foo", 1u);
  const Event* foo_event2 = model_->GetEvent("foo");
  EXPECT_EQ(1, foo_event2->events_size());
  test::VerifyEventCount(foo_event2, 1u, 2u);
  test::VerifyEventsEqual(foo_event2, store_->GetLastWrittenEvent());
}

TEST_F(EventModelImplTest, IncrementingSingleDayExistingEventTwice) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // |foo| is inserted into the store with a count of 1 at day 1, so
  // incrementing twice should lead to 3.
  model_->IncrementEvent("foo", 1u);
  model_->IncrementEvent("foo", 1u);
  const Event* foo_event = model_->GetEvent("foo");
  EXPECT_EQ(1, foo_event->events_size());
  test::VerifyEventCount(foo_event, 1u, 3u);
  test::VerifyEventsEqual(foo_event, store_->GetLastWrittenEvent());
}

TEST_F(EventModelImplTest, IncrementingExistingMultiDayEvent) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // |bar| is inserted into the store with a count of 3 at day 2. Incrementing
  // that day should lead to a count of 4.
  const Event* bar_event = model_->GetEvent("bar");
  test::VerifyEventCount(bar_event, 2u, 3u);
  model_->IncrementEvent("bar", 2u);
  const Event* bar_event2 = model_->GetEvent("bar");
  test::VerifyEventCount(bar_event2, 2u, 4u);
  test::VerifyEventsEqual(bar_event2, store_->GetLastWrittenEvent());
}

TEST_F(EventModelImplTest, IncrementingExistingMultiDayEventNewDay) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // |bar| does not contain entries for day 10, so incrementing should create
  // the day.
  model_->IncrementEvent("bar", 10u);
  const Event* bar_event = model_->GetEvent("bar");
  test::VerifyEventCount(bar_event, 10u, 1u);
  test::VerifyEventsEqual(bar_event, store_->GetLastWrittenEvent());
  model_->IncrementEvent("bar", 10u);
  const Event* bar_event2 = model_->GetEvent("bar");
  test::VerifyEventCount(bar_event2, 10u, 2u);
  test::VerifyEventsEqual(bar_event2, store_->GetLastWrittenEvent());
}

TEST_F(EventModelImplTest, IncrementingSnoozeEvent) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // Verify that incrementing snooze across multiple days update the snooze
  // count and the last_snooze_time_us field.
  base::Time snooze_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5));
  model_->IncrementEvent("snooze", 1u);
  model_->IncrementSnooze("snooze", 1u, base::Time());
  model_->IncrementEvent("snooze", 2u);
  model_->IncrementEvent("snooze", 3u);
  model_->IncrementSnooze("snooze", 3u, base::Time());
  model_->IncrementEvent("snooze", 3u);
  model_->IncrementSnooze("snooze", 3u, snooze_time);
  model_->IncrementEvent("snooze", 5u);
  const Event* bar_event = model_->GetEvent("snooze");
  EXPECT_EQ(snooze_time.ToDeltaSinceWindowsEpoch().InMicroseconds(),
            bar_event->last_snooze_time_us());
  EXPECT_EQ(0u, model_->GetSnoozeCount("snooze", 1u, 5u));
  EXPECT_EQ(2u, model_->GetSnoozeCount("snooze", 3u, 5u));
  EXPECT_EQ(3u, model_->GetSnoozeCount("snooze", 5u, 5u));
  EXPECT_EQ(2u, model_->GetEventCount("snooze", 5u, 5u));
}

TEST_F(EventModelImplTest, DismissSnoozeEvent) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // Verify that dismissing a snooze event update the snooze_dismissed flag.
  model_->DismissSnooze("bar");
  EXPECT_EQ(true, model_->IsSnoozeDismissed("bar"));
}

TEST_F(EventModelImplTest, GetLastSnoozeTimestamp) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // Verify the correct last_snooze_time_us is returned.
  base::Time snooze_time1 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(4));
  base::Time snooze_time2 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5));

  model_->IncrementSnooze("bar", 10u, snooze_time1);
  EXPECT_EQ(snooze_time1, model_->GetLastSnoozeTimestamp("bar"));
  model_->IncrementSnooze("bar", 10u, snooze_time2);
  EXPECT_EQ(snooze_time2, model_->GetLastSnoozeTimestamp("bar"));
}

TEST_F(EventModelImplTest, GetEventCount) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  // Verify counts with different window size.
  uint32_t current_day = 6;
  EXPECT_EQ(model_->GetEventCount("bar", current_day, 0u), 0u);
  EXPECT_EQ(model_->GetEventCount("bar", current_day, 1u), 0u);
  EXPECT_EQ(model_->GetEventCount("bar", current_day, 2u), 5u);
  EXPECT_EQ(model_->GetEventCount("bar", current_day, 3u), 5u);
  EXPECT_EQ(model_->GetEventCount("bar", current_day, 5u), 8u);
  EXPECT_EQ(model_->GetEventCount("bar", current_day, 6u), 11u);

  // Verify window_size > current_day.
  EXPECT_EQ(model_->GetEventCount("bar", current_day, 100u), 11u);

  // Verify counts with different reference date.
  uint32_t window_size = 5u;
  EXPECT_EQ(model_->GetEventCount("bar", 5u, window_size), 11u);
  EXPECT_EQ(model_->GetEventCount("bar", 6u, window_size), 8u);
  EXPECT_EQ(model_->GetEventCount("bar", 7u, window_size), 5u);
  EXPECT_EQ(model_->GetEventCount("bar", 9u, window_size), 5u);
  EXPECT_EQ(model_->GetEventCount("bar", 10u, window_size), 0u);

  // Verify counts for non existing event is always 0.
  EXPECT_EQ(model_->GetEventCount("nonexisting", 100u, 100u), 0u);
}

TEST_F(LoadFailingEventModelImplTest, FailedInitializeInformsCaller) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(model_->IsReady());
  EXPECT_TRUE(got_initialize_callback_);
  EXPECT_FALSE(initialize_callback_result_);
}

TEST_F(EventModelImplTest, ClearEvents) {
  model_->Initialize(
      base::BindOnce(&EventModelImplTest::OnModelInitializationFinished,
                     base::Unretained(this)),
      1000u);
  task_runner_->RunUntilIdle();
  EXPECT_TRUE(model_->IsReady());

  EXPECT_NE(nullptr, model_->GetEvent("foo"));
  EXPECT_NE(0U, model_->GetEventCount("foo", 5U, 5U));
  EXPECT_NE(nullptr, model_->GetEvent("bar"));
  EXPECT_NE(0U, model_->GetEventCount("bar", 5U, 5U));

  model_->ClearEvent("foo");

  // ClearEvent() does not remove the event, but clears all the instances.
  EXPECT_NE(nullptr, model_->GetEvent("foo"));
  EXPECT_EQ(0U, model_->GetEventCount("foo", 5U, 5U));
  EXPECT_NE(nullptr, model_->GetEvent("bar"));
  EXPECT_NE(0U, model_->GetEventCount("bar", 5U, 5U));
}

}  // namespace feature_engagement
