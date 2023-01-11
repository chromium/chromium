// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chromecast/base/observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

class ObserverTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

struct NoDefaultConstructor {
  NoDefaultConstructor(int v) : value(v) {}

  int value;
};

class ThreadedObservable {
 public:
  ThreadedObservable() : thread_("ThreadedObservable"), value_(0) {
    thread_.Start();
  }

  ThreadedObservable(const ThreadedObservable&) = delete;
  ThreadedObservable& operator=(const ThreadedObservable&) = delete;

  Observer<int> Observe() { return value_.Observe(); }

  void SetValue(int value) {
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ThreadedObservable::SetValueOnThread,
                                  base::Unretained(this), value));
  }

 private:
  void SetValueOnThread(int value) {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());
    value_.SetValue(value);
  }

  base::Thread thread_;
  Observable<int> value_;
};

class ThreadedObserver {
 public:
  ThreadedObserver()
      : thread_("ThreadedObserver"),
        observing_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {
    thread_.Start();
  }

  ThreadedObserver(const ThreadedObserver&) = delete;
  ThreadedObserver& operator=(const ThreadedObserver&) = delete;

  ~ThreadedObserver() {
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ThreadedObserver::DestroyOnThread,
                                  base::Unretained(this)));
    thread_.Stop();
  }

  void Observe(Observable<int>* observable) {
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ThreadedObserver::ObserveOnThread,
                                  base::Unretained(this), observable));
    observing_.Wait();
  }

  void CheckValue(int value) {
    thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ThreadedObserver::CheckValueOnThread,
                                  base::Unretained(this), value));
  }

 private:
  void ObserveOnThread(Observable<int>* observable) {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());
    observer_ = std::make_unique<Observer<int>>(observable->Observe());
    observing_.Signal();
  }

  void CheckValueOnThread(int value) {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());
    EXPECT_EQ(value, observer_->GetValue());
  }

  void DestroyOnThread() {
    DCHECK(thread_.task_runner()->BelongsToCurrentThread());
    observer_.reset();
  }

  base::Thread thread_;
  std::unique_ptr<Observer<int>> observer_;
  base::WaitableEvent observing_;
};

void RunCallback(std::function<void()> callback) {
  callback();
}

TEST_F(ObserverTest, SimpleValue) {
  Observable<int> original(0);
  Observer<int> observer = original.Observe();

  EXPECT_EQ(0, observer.GetValue());

  original.SetValue(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.GetValue());
}

TEST_F(ObserverTest, MultipleObservers) {
  Observable<int> original(0);
  Observer<int> observer1 = original.Observe();
  Observer<int> observer2 = observer1;

  EXPECT_EQ(0, observer1.GetValue());
  EXPECT_EQ(0, observer2.GetValue());

  original.SetValue(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer1.GetValue());
  EXPECT_EQ(1, observer2.GetValue());
}

TEST_F(ObserverTest, NoDefaultConstructor) {
  Observable<NoDefaultConstructor> original(0);
  Observer<NoDefaultConstructor> observer = original.Observe();

  EXPECT_EQ(0, observer.GetValue().value);

  original.SetValue(1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.GetValue().value);
}

TEST_F(ObserverTest, NoMissingEvents) {
  Observable<int> original(0);
  Observer<int> observer = original.Observe();
  original.SetValue(1);

  std::vector<int> event_values;
  std::function<void()> callback = [&]() {
    event_values.push_back(observer.GetValue());
  };
  observer.SetOnUpdateCallback(base::BindRepeating(&RunCallback, callback));

  EXPECT_EQ(0, observer.GetValue());

  original.SetValue(2);
  base::RunLoop().RunUntilIdle();
  original.SetValue(3);
  original.SetValue(4);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(4u, event_values.size());
  EXPECT_EQ(1, event_values[0]);
  EXPECT_EQ(2, event_values[1]);
  EXPECT_EQ(3, event_values[2]);
  EXPECT_EQ(4, event_values[3]);

  EXPECT_EQ(4, observer.GetValue());
}

TEST_F(ObserverTest, NoExtraEventsAfterChange) {
  Observable<int> original(0);
  original.SetValue(1);

  Observer<int> observer = original.Observe();
  EXPECT_EQ(1, observer.GetValue());

  std::vector<int> event_values;
  std::function<void()> callback = [&]() {
    event_values.push_back(observer.GetValue());
  };
  observer.SetOnUpdateCallback(base::BindRepeating(&RunCallback, callback));

  // Propagate the SetValue event; the observer shouldn't get it since it
  // started observing after SetValue().
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.GetValue());
  EXPECT_EQ(0u, event_values.size());
}

TEST_F(ObserverTest, NoExtraEventsBetweenChanges) {
  Observable<int> original(0);
  original.SetValue(1);

  Observer<int> observer = original.Observe();
  EXPECT_EQ(1, observer.GetValue());

  original.SetValue(2);

  std::vector<int> event_values;
  std::function<void()> callback = [&]() {
    event_values.push_back(observer.GetValue());
  };
  observer.SetOnUpdateCallback(base::BindRepeating(&RunCallback, callback));

  // Propagate the SetValue events; the observer should only get the second
  // event, corresponding to the SetValue after the observer was created.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, observer.GetValue());
  ASSERT_EQ(1u, event_values.size());
  EXPECT_EQ(2, event_values[0]);
}

TEST_F(ObserverTest, NoExtraEventsForCopy) {
  Observable<int> original(0);
  original.SetValue(1);

  Observer<int> observer1 = original.Observe();
  EXPECT_EQ(1, observer1.GetValue());

  original.SetValue(2);

  Observer<int> observer2 = observer1;
  // All observers on the same thread observe the same value. The update hasn't
  // propagated yet.
  EXPECT_EQ(1, observer2.GetValue());

  std::vector<int> event_values1;
  std::function<void()> callback1 = [&]() {
    event_values1.push_back(observer1.GetValue());
  };
  observer1.SetOnUpdateCallback(base::BindRepeating(&RunCallback, callback1));

  std::vector<int> event_values2;
  std::function<void()> callback2 = [&]() {
    event_values2.push_back(observer2.GetValue());
  };
  observer2.SetOnUpdateCallback(base::BindRepeating(&RunCallback, callback2));

  // Propagate the SetValue events; each observer should get just one callback
  // for the new value.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, observer1.GetValue());
  EXPECT_EQ(2, observer2.GetValue());

  ASSERT_EQ(1u, event_values1.size());
  EXPECT_EQ(2, event_values1[0]);

  ASSERT_EQ(1u, event_values2.size());
  EXPECT_EQ(2, event_values2[0]);
}

TEST_F(ObserverTest, SetCallbackTwice) {
  Observable<int> original(0);
  original.SetValue(1);

  Observer<int> observer = original.Observe();
  EXPECT_EQ(1, observer.GetValue());

  original.SetValue(2);

  std::vector<int> event_values1;
  std::function<void()> callback1 = [&]() {
    event_values1.push_back(observer.GetValue());
  };
  observer.SetOnUpdateCallback(base::BindRepeating(&RunCallback, callback1));

  std::vector<int> event_values2;
  std::function<void()> callback2 = [&]() {
    event_values2.push_back(observer.GetValue());
  };
  observer.SetOnUpdateCallback(base::BindRepeating(&RunCallback, callback2));

  // Propagate the SetValue events; only the second callback should be run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, observer.GetValue());
  EXPECT_EQ(0u, event_values1.size());
  ASSERT_EQ(1u, event_values2.size());
  EXPECT_EQ(2, event_values2[0]);
}

TEST_F(ObserverTest, ObserverOutlivesObservable) {
  auto original = std::make_unique<Observable<int>>(0);
  Observer<int> observer1 = original->Observe();

  EXPECT_EQ(0, observer1.GetValue());

  original->SetValue(1);
  original.reset();

  Observer<int> observer2 = observer1;

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, observer1.GetValue());
  EXPECT_EQ(1, observer2.GetValue());
}

TEST_F(ObserverTest, ObserverOnDifferentThread) {
  auto original = std::make_unique<ThreadedObservable>();
  Observer<int> observer = original->Observe();
  EXPECT_EQ(0, observer.GetValue());

  std::vector<int> event_values;
  std::function<void()> callback = [&]() {
    event_values.push_back(observer.GetValue());
  };
  observer.SetOnUpdateCallback(base::BindRepeating(&RunCallback, callback));

  original->SetValue(1);
  original->SetValue(2);
  original.reset();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, observer.GetValue());
  ASSERT_EQ(2u, event_values.size());
  EXPECT_EQ(1, event_values[0]);
  EXPECT_EQ(2, event_values[1]);
}

TEST_F(ObserverTest, ObserveOnManyThreads) {
  auto original = std::make_unique<Observable<int>>(0);
  std::vector<std::unique_ptr<ThreadedObserver>> observers;
  for (int i = 0; i < 20; ++i) {
    observers.push_back(std::make_unique<ThreadedObserver>());
    observers.back()->Observe(original.get());
  }

  original->SetValue(1);
  original.reset();

  base::RunLoop().RunUntilIdle();
  for (auto& observer : observers) {
    observer->CheckValue(1);
  }

  // Deleting the observers should check the expectations, since all posted
  // tasks on their internal threads will run.
  observers.clear();
}

}  // namespace chromecast
