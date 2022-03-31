// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "components/reporting/resources/resource_interface.h"

#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting {
namespace {

class ResourceInterfaceTest
    : public ::testing::TestWithParam<ResourceInterface*> {
 protected:
  ResourceInterface* resource_interface() const { return GetParam(); }

  void TearDown() override {
    EXPECT_THAT(resource_interface()->GetUsed(), Eq(0u));
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_P(ResourceInterfaceTest, NestedReservationTest) {
  uint64_t size = resource_interface()->GetTotal();
  while ((size / 2) > 0u) {
    size /= 2;
    EXPECT_TRUE(resource_interface()->Reserve(size));
  }

  for (; size < resource_interface()->GetTotal(); size *= 2) {
    resource_interface()->Discard(size);
  }
}

TEST_P(ResourceInterfaceTest, SimultaneousReservationTest) {
  uint64_t size = resource_interface()->GetTotal();

  // Schedule reservations.
  test::TestCallbackWaiter reserve_waiter;
  while ((size / 2) > 0u) {
    size /= 2;
    reserve_waiter.Attach();
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](size_t size, ResourceInterface* resource_interface,
               test::TestCallbackWaiter* waiter) {
              EXPECT_TRUE(resource_interface->Reserve(size));
              waiter->Signal();
            },
            size, resource_interface(), &reserve_waiter));
  }
  reserve_waiter.Wait();

  // Schedule discards.
  test::TestCallbackWaiter discard_waiter;
  for (; size < resource_interface()->GetTotal(); size *= 2) {
    discard_waiter.Attach();
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](size_t size, ResourceInterface* resource_interface,
               test::TestCallbackWaiter* waiter) {
              resource_interface->Discard(size);
              waiter->Signal();
            },
            size, resource_interface(), &discard_waiter));
  }
  discard_waiter.Wait();
}

TEST_P(ResourceInterfaceTest, SimultaneousScopedReservationTest) {
  uint64_t size = resource_interface()->GetTotal();
  test::TestCallbackWaiter waiter;
  while ((size / 2) > 0u) {
    size /= 2;
    waiter.Attach();
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](size_t size, ResourceInterface* resource_interface,
               test::TestCallbackWaiter* waiter) {
              { ScopedReservation(size, resource_interface); }
              waiter->Signal();
            },
            size, resource_interface(), &waiter));
  }
  waiter.Wait();
}

TEST_P(ResourceInterfaceTest, MoveScopedReservationTest) {
  uint64_t size = resource_interface()->GetTotal();
  ScopedReservation scoped_reservation(size / 2, resource_interface());
  EXPECT_TRUE(scoped_reservation.reserved());
  {
    ScopedReservation moved_scoped_reservation(std::move(scoped_reservation));
    EXPECT_TRUE(moved_scoped_reservation.reserved());
    EXPECT_FALSE(scoped_reservation.reserved());
  }
  EXPECT_FALSE(scoped_reservation.reserved());
}

TEST_P(ResourceInterfaceTest, ScopedReservationBasicReduction) {
  uint64_t size = resource_interface()->GetTotal() / 2;
  ScopedReservation scoped_reservation(size, resource_interface());
  EXPECT_TRUE(scoped_reservation.reserved());
  EXPECT_TRUE(scoped_reservation.Reduce(size / 2));
}

TEST_P(ResourceInterfaceTest, ScopedReservationReductionWithLargerNewSize) {
  uint64_t size = resource_interface()->GetTotal() / 2;
  ScopedReservation scoped_reservation(size, resource_interface());
  EXPECT_TRUE(scoped_reservation.reserved());
  EXPECT_FALSE(scoped_reservation.Reduce(size + 1));
}

TEST_P(ResourceInterfaceTest, ScopedReservationReductionWithNegativeNewSize) {
  uint64_t size = resource_interface()->GetTotal() / 2;
  ScopedReservation scoped_reservation(size, resource_interface());
  EXPECT_TRUE(scoped_reservation.reserved());
  EXPECT_FALSE(scoped_reservation.Reduce(-(size / 2)));
}

TEST_P(ResourceInterfaceTest, ScopedReservationRepeatingReductions) {
  uint64_t size = resource_interface()->GetTotal() / 2;
  ScopedReservation scoped_reservation(size, resource_interface());
  EXPECT_TRUE(scoped_reservation.reserved());

  for (; size >= 2; size /= 2) {
    EXPECT_TRUE(scoped_reservation.Reduce(size / 2));
  }
  EXPECT_FALSE(scoped_reservation.Reduce(size / 2));
}

TEST_P(ResourceInterfaceTest, ReservationOverMaxTest) {
  EXPECT_FALSE(
      resource_interface()->Reserve(resource_interface()->GetTotal() + 1));
  EXPECT_TRUE(resource_interface()->Reserve(resource_interface()->GetTotal()));
  resource_interface()->Discard(resource_interface()->GetTotal());
}

INSTANTIATE_TEST_SUITE_P(VariousResources,
                         ResourceInterfaceTest,
                         testing::Values(GetMemoryResource(),
                                         GetDiskResource()));

}  // namespace
}  // namespace reporting
