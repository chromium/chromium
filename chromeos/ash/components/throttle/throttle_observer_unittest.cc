// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/throttle/throttle_observer.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

constexpr const char kObserverName[] = "TestObserver";

class ThrottleObserverTest : public testing::Test {
 public:
  ThrottleObserverTest() {
    observer_.StartObserving(
        nullptr /* content::BrowserContext* */,
        base::BindRepeating(&ThrottleObserverTest::OnObserverStateChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  ThrottleObserverTest(const ThrottleObserverTest&) = delete;
  ThrottleObserverTest& operator=(const ThrottleObserverTest&) = delete;

  void OnObserverStateChanged(const ThrottleObserver*) { notify_count_++; }

 protected:
  ThrottleObserver* observer() { return &observer_; }
  size_t notify_count() const { return notify_count_; }

 private:
  ThrottleObserver observer_{kObserverName};
  size_t notify_count_{0};
  base::WeakPtrFactory<ThrottleObserverTest> weak_ptr_factory_{this};
};

// Tests that ThrottleObserver can be constructed and destructed.
TEST_F(ThrottleObserverTest, TestConstructDestruct) {}

// Tests that ThrottleObserver's name is properly set.
TEST_F(ThrottleObserverTest, TestObserverName) {
  EXPECT_EQ(kObserverName, observer()->name());
}

// Tests that ThrottleObserver notifies observers on SetActive().
TEST_F(ThrottleObserverTest, TestSetActive) {
  EXPECT_EQ(0U, notify_count());
  EXPECT_FALSE(observer()->active());

  observer()->SetActive(true);
  EXPECT_TRUE(observer()->active());
  EXPECT_EQ(1U, notify_count());

  observer()->SetActive(true);
  EXPECT_TRUE(observer()->active());
  EXPECT_EQ(2U, notify_count());

  observer()->SetActive(false);
  EXPECT_FALSE(observer()->active());
  EXPECT_EQ(3U, notify_count());
}

// Tests that ThrottleObserver notifies observers on SetEnforced().
TEST_F(ThrottleObserverTest, TestSetEnforced) {
  EXPECT_EQ(0U, notify_count());
  EXPECT_FALSE(observer()->enforced());

  observer()->SetEnforced(true);
  EXPECT_TRUE(observer()->enforced());
  EXPECT_EQ(1U, notify_count());

  observer()->SetEnforced(true);
  EXPECT_TRUE(observer()->enforced());
  EXPECT_EQ(2U, notify_count());

  observer()->SetEnforced(false);
  EXPECT_FALSE(observer()->enforced());
  EXPECT_EQ(3U, notify_count());
}

// Tests that the callback is not called after StopObserving.
TEST_F(ThrottleObserverTest, TestStopObserving) {
  observer()->StopObserving();
  EXPECT_EQ(0U, notify_count());

  observer()->SetActive(true);
  EXPECT_EQ(0U, notify_count());

  observer()->SetEnforced(true);
  EXPECT_EQ(0U, notify_count());
}

}  // namespace ash
