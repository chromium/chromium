// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/input_budget.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {
namespace {

class InputBudgetTest : public testing::Test {
 protected:
  // Reserves `bytes`, and keeps the reservation in `reservations_[id]` when it
  // arrives. Records a rejection in `rejected_`.
  void Reserve(size_t bytes, int id) {
    budget_->Reserve(bytes, base::BindOnce(
                                [](InputBudgetTest* test, int id,
                                   std::optional<InputBudget::Reservation> r) {
                                  if (!r) {
                                    test->rejected_.push_back(id);
                                    return;
                                  }
                                  test->order_.push_back(id);
                                  test->reservations_.emplace(id,
                                                              std::move(*r));
                                },
                                this, id));
  }

  void Release(int id) {
    reservations_.erase(id);
    task_environment_.RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<InputBudget> budget_ =
      base::MakeRefCounted<InputBudget>(/*max_bytes=*/10,
                                        /*max_queued_bytes=*/12);
  std::map<int, InputBudget::Reservation> reservations_;
  std::vector<int> order_;
  std::vector<int> rejected_;
};

TEST_F(InputBudgetTest, ReservesAtOnceWhenItFits) {
  Reserve(6, 1);
  Reserve(4, 2);
  EXPECT_EQ(order_, (std::vector<int>{1, 2}));
  EXPECT_EQ(budget_->reserved_bytes(), 10u);
  EXPECT_EQ(budget_->queued_bytes(), 0u);
}

TEST_F(InputBudgetTest, WaitsInOrderAndRunsWhenBudgetComesBack) {
  Reserve(6, 1);
  Reserve(6, 2);
  // Fits, but waits behind 2 to keep the order.
  Reserve(1, 3);
  EXPECT_EQ(order_, (std::vector<int>{1}));
  EXPECT_EQ(budget_->queued_bytes(), 7u);

  Release(1);
  EXPECT_EQ(order_, (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(budget_->reserved_bytes(), 7u);
  EXPECT_EQ(budget_->queued_bytes(), 0u);
}

TEST_F(InputBudgetTest, RejectsWhenTheQueueIsFull) {
  Reserve(10, 1);
  Reserve(6, 2);
  Reserve(6, 3);
  Reserve(1, 4);
  EXPECT_EQ(rejected_, (std::vector<int>{4}));
  EXPECT_EQ(budget_->queued_bytes(), 12u);
}

TEST_F(InputBudgetTest, LargeInputRunsAlone) {
  Reserve(4, 1);
  Reserve(11, 2);
  EXPECT_EQ(order_, (std::vector<int>{1}));
  Release(1);
  EXPECT_EQ(order_, (std::vector<int>{1, 2}));
  EXPECT_EQ(budget_->reserved_bytes(), 11u);

  // While an input larger than `max_bytes` is in flight, later requests must
  // wait rather than bypassing the budget via unsigned underflow.
  Reserve(4, 3);
  EXPECT_EQ(order_, (std::vector<int>{1, 2}));
  EXPECT_EQ(budget_->queued_bytes(), 4u);

  Release(2);
  EXPECT_EQ(order_, (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(budget_->reserved_bytes(), 4u);
  EXPECT_EQ(budget_->queued_bytes(), 0u);
}

TEST_F(InputBudgetTest, CancelledWaiterGivesUpItsPlace) {
  Reserve(10, 1);
  bool ran = false;
  struct Target {
    void Run(bool* ran, std::optional<InputBudget::Reservation>) {
      *ran = true;
    }
    base::WeakPtrFactory<Target> weak_factory{this};
  };
  auto target = std::make_unique<Target>();
  budget_->Reserve(
      6, base::BindOnce(&Target::Run, target->weak_factory.GetWeakPtr(), &ran));
  target.reset();
  Reserve(4, 2);
  Release(1);
  EXPECT_FALSE(ran);
  EXPECT_EQ(order_, (std::vector<int>{1, 2}));
}

TEST_F(InputBudgetTest, ReservePrunesCancelledWaitersBeforeCapacityCheck) {
  budget_ = base::MakeRefCounted<InputBudget>(
      /*max_bytes=*/10, /*max_queued_bytes=*/12,
      /*min_charge_bytes=*/0, /*max_waiters=*/2);
  Reserve(10, 1);
  Reserve(6, 2);

  bool ran = false;
  struct Target {
    void Run(bool* ran, std::optional<InputBudget::Reservation>) {
      *ran = true;
    }
    base::WeakPtrFactory<Target> weak_factory{this};
  };
  auto target = std::make_unique<Target>();
  budget_->Reserve(
      6, base::BindOnce(&Target::Run, target->weak_factory.GetWeakPtr(), &ran));
  EXPECT_EQ(budget_->queued_bytes(), 12u);

  // Destroying the queued waiter's owner frees both its byte and waiter slots
  // when the next Reserve hits capacity.
  target.reset();
  Reserve(6, 3);
  EXPECT_TRUE(rejected_.empty());
  EXPECT_EQ(budget_->queued_bytes(), 12u);

  Release(1);
  EXPECT_FALSE(ran);
  EXPECT_EQ(order_, (std::vector<int>{1, 2}));
  Release(2);
  EXPECT_EQ(order_, (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(budget_->queued_bytes(), 0u);
}

TEST_F(InputBudgetTest, ReserveRunsImmediatelyAfterCancelledFrontWaiter) {
  Reserve(4, 1);
  bool ran = false;
  struct Target {
    void Run(bool* ran, std::optional<InputBudget::Reservation>) {
      *ran = true;
    }
    base::WeakPtrFactory<Target> weak_factory{this};
  };
  auto target = std::make_unique<Target>();
  budget_->Reserve(
      8, base::BindOnce(&Target::Run, target->weak_factory.GetWeakPtr(), &ran));
  EXPECT_EQ(budget_->queued_bytes(), 8u);

  target.reset();
  Reserve(4, 2);
  EXPECT_FALSE(ran);
  EXPECT_EQ(order_, (std::vector<int>{1, 2}));
  EXPECT_EQ(budget_->reserved_bytes(), 8u);
  EXPECT_EQ(budget_->queued_bytes(), 0u);
}

TEST_F(InputBudgetTest, MovedReservationGivesTheBudgetBackOnce) {
  Reserve(4, 1);
  Reserve(3, 2);
  InputBudget::Reservation moved(std::move(reservations_.at(1)));
  reservations_.erase(1);
  EXPECT_EQ(budget_->reserved_bytes(), 7u);
  // Assigning gives back what `moved` held, and takes over 2.
  moved = std::move(reservations_.at(2));
  reservations_.erase(2);
  EXPECT_EQ(budget_->reserved_bytes(), 3u);
}

TEST_F(InputBudgetTest, ReservationKeepsTheBudgetAlive) {
  base::WeakPtr<InputBudget> weak_budget = budget_->GetWeakPtr();
  Reserve(5, 1);
  budget_.reset();
  EXPECT_TRUE(weak_budget);
  reservations_.clear();
  EXPECT_FALSE(weak_budget);
}

TEST_F(InputBudgetTest, ChargesAtLeastMinChargeBytes) {
  budget_ = base::MakeRefCounted<InputBudget>(
      /*max_bytes=*/2048, /*max_queued_bytes=*/2048,
      /*min_charge_bytes=*/1024);
  Reserve(1, 1);
  Reserve(1, 2);
  EXPECT_EQ(order_, (std::vector<int>{1, 2}));
  EXPECT_EQ(budget_->reserved_bytes(), 2048u);

  Reserve(1, 3);
  Reserve(1, 4);
  Reserve(1, 5);
  EXPECT_EQ(budget_->queued_bytes(), 2048u);
  EXPECT_EQ(rejected_, (std::vector<int>{5}));

  Release(1);
  EXPECT_EQ(order_, (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(budget_->reserved_bytes(), 2048u);
  EXPECT_EQ(budget_->queued_bytes(), 1024u);

  Release(2);
  Release(3);
  Release(4);
  EXPECT_EQ(budget_->reserved_bytes(), 0u);
  EXPECT_EQ(budget_->queued_bytes(), 0u);
}

TEST_F(InputBudgetTest, RejectsWhenWaiterCapIsReached) {
  budget_ = base::MakeRefCounted<InputBudget>(
      /*max_bytes=*/10, /*max_queued_bytes=*/1000,
      /*min_charge_bytes=*/0, /*max_waiters=*/2);
  Reserve(10, 1);
  Reserve(1, 2);
  Reserve(1, 3);
  Reserve(1, 4);
  EXPECT_EQ(order_, (std::vector<int>{1}));
  EXPECT_EQ(rejected_, (std::vector<int>{4}));
  EXPECT_EQ(budget_->queued_bytes(), 2u);

  Release(1);
  EXPECT_EQ(order_, (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(budget_->reserved_bytes(), 2u);
  EXPECT_EQ(budget_->queued_bytes(), 0u);
}

}  // namespace
}  // namespace on_device_translation
