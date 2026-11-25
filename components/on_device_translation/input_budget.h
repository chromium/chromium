// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_INPUT_BUDGET_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_INPUT_BUDGET_H_

#include <cstddef>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

namespace on_device_translation {

// Limits how many bytes of translation input an origin has sent to its
// translation service that the service hasn't answered yet.
//
// The service translates one call at a time, and the calls that wait for it
// queue in the service process. A per-call size limit doesn't bound that
// queue, because a page can send many calls without waiting for the results,
// from one or several translators. Every `Translator` of an origin shares one
// `InputBudget`, and reserves the size of each input before it sends it.
// Calls that don't fit wait in the browser, in the order they came, until
// earlier calls return. Calls that would make the browser queue too large
// fail.
class InputBudget : public base::RefCounted<InputBudget> {
 public:
  // Holds a part of the budget, and gives it back when it goes away.
  class Reservation {
   public:
    Reservation(Reservation&& other) noexcept;
    Reservation& operator=(Reservation&& other) noexcept;
    ~Reservation();

   private:
    friend class InputBudget;
    Reservation(scoped_refptr<InputBudget> budget, size_t bytes);

    scoped_refptr<InputBudget> budget_;
    size_t bytes_ = 0;
  };

  // Runs with the reservation, or with nothing if the call can't wait.
  using ReserveCallback =
      base::OnceCallback<void(std::optional<Reservation> reservation)>;

  static constexpr size_t kDefaultMaxWaiters = 1024;

  // `max_bytes` is the budget for calls sent to the service.
  // `max_queued_bytes` is the most input that may wait in the browser.
  // `min_charge_bytes` sets a floor on each call's budget charge so many tiny
  // calls cannot bypass byte accounting, and `max_waiters` caps pending queue
  // entries.
  InputBudget(size_t max_bytes,
              size_t max_queued_bytes,
              size_t min_charge_bytes = 0,
              size_t max_waiters = kDefaultMaxWaiters);

  InputBudget(const InputBudget&) = delete;
  InputBudget& operator=(const InputBudget&) = delete;

  // Reserves `bytes` of the budget, and runs `callback` with the reservation.
  // When nothing waits and `bytes` fit, `callback` runs before this returns.
  // Otherwise the call waits behind the earlier ones, and `callback` runs
  // later, in order. If the waiting input would go over `max_queued_bytes`,
  // `callback` runs at once with nothing. An input larger than `max_bytes`
  // runs alone, when nothing else is reserved. A cancelled `callback`, for
  // example one bound to a destroyed object, gives up its place.
  void Reserve(size_t bytes, ReserveCallback callback);

  size_t reserved_bytes() const { return reserved_bytes_; }
  size_t queued_bytes() const { return queued_bytes_; }

  base::WeakPtr<InputBudget> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class base::RefCounted<InputBudget>;

  struct Waiter {
    Waiter(size_t bytes, ReserveCallback callback);
    Waiter(Waiter&&);
    Waiter& operator=(Waiter&&);
    ~Waiter();

    size_t bytes;
    ReserveCallback callback;
  };

  ~InputBudget();

  bool Fits(size_t bytes) const;
  // Drops cancelled waiters and updates `queued_bytes_`.
  void PruneCancelledWaiters();
  // Called by `Reservation`.
  void GiveBack(size_t bytes);
  // Gives the budget to the waiting calls that fit, in order.
  void RunWaiters();

  const size_t max_bytes_;
  const size_t max_queued_bytes_;
  const size_t min_charge_bytes_;
  const size_t max_waiters_;
  size_t reserved_bytes_ = 0;
  size_t queued_bytes_ = 0;
  base::circular_deque<Waiter> waiters_;
  bool run_waiters_posted_ = false;
  base::WeakPtrFactory<InputBudget> weak_ptr_factory_{this};
};

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_INPUT_BUDGET_H_
