// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_RESOURCES_RESOURCE_MANAGER_H_
#define COMPONENTS_REPORTING_RESOURCES_RESOURCE_MANAGER_H_

#include <atomic>
#include <cstdint>
#include <optional>
#include <queue>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"

namespace reporting {

// Resource management class. The class is thread-safe.
// Each resource instance is created with its own total size; the rest of the
// functionality is identical. All APIs are non-blocking.
class ResourceManager : public base::RefCountedThreadSafe<ResourceManager> {
 public:
  explicit ResourceManager(uint64_t total_size);

  // Needs to be called before attempting to allocate specified size.
  // Returns true if requested amount can be allocated.
  // After that the caller can actually allocate it or must call
  // |Discard| if decided not to allocate.
  bool Reserve(uint64_t size);

  // Reverts reservation, arranges for callbacks calls as necessary.
  // Must be called after the specified amount is released.
  void Discard(uint64_t size);

  // Returns total amount.
  uint64_t GetTotal() const;

  // Returns current used amount.
  uint64_t GetUsed() const;

  // Registers a callback to be invoked once there is specified amount
  // of resource available (does not reserve it, so once called back
  // the respective code must attempt to reserve again, and if unsuccessful,
  // may need ot re-register the callback).
  // Callbacks will be invoked in the context of the sequenced task runner
  // it was registered in.
  void RegisterCallback(uint64_t size, base::OnceClosure cb);

  // Test only: Sets non-default usage limit.
  void Test_SetTotal(uint64_t test_total);

 private:
  friend class base::RefCountedThreadSafe<ResourceManager>;

  ~ResourceManager();

  // Flushes as many callbacks as possible given the current resource
  // availability. Callbacks only signal that resource may be available,
  // the resumed task must try to actually reserve it after that.
  void FlushCallbacks();

  uint64_t total_;  // Remains constant in prod code, changes only in tests.
  std::atomic<uint64_t> used_{0};

  // Sequenced task runner for callbacks handling (not for calling callbacks!)
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Queue of pairs [size, callback].
  // When `Discard` leaves enough space available (even momentarily),
  // calls as many of the callbacks as fit in that size, in the queue order.
  // Note that in a meantime reservation may change - the called back code
  // must attempt reservation before using it.
  std::queue<std::pair<uint64_t, base::OnceClosure>> resource_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

// Moveable RAII class used for scoped Reserve-Discard.
//
// Usage:
//  {
//    ScopedReservation reservation(1024u, options.memory_resource());
//    if (!reservation.reserved()) {
//      // Allocation failed.
//      return;
//    }
//    // Allocation succeeded.
//    ...
//  }   // Automatically discarded.
//
// Can be handed over to another owner by move-constructor or using HandOver
// method:
// {
//   ScopedReservation summary;
//   for (const uint64_t size : sizes) {
//     ScopedReservation single_reservation(size, resource);
//     ...
//     summary.HandOver(single_reservation);
//   }
// }
class ScopedReservation {
 public:
  // Zero-size reservation with no resource interface attached.
  // reserved() returns false.
  ScopedReservation() noexcept;
  // Specified reservation, must have resource interface attached.
  ScopedReservation(uint64_t size,
                    scoped_refptr<ResourceManager> resource_manager) noexcept;
  // New reservation on the same resource interface as |other_reservation|.
  ScopedReservation(uint64_t size,
                    const ScopedReservation& other_reservation) noexcept;
  // Move constructor.
  ScopedReservation(ScopedReservation&& other) noexcept;
  ScopedReservation(const ScopedReservation& other) = delete;
  ScopedReservation& operator=(ScopedReservation&& other) = delete;
  ScopedReservation& operator=(const ScopedReservation& other) = delete;
  ~ScopedReservation();

  bool reserved() const;

  // Reduces reservation to |new_size|.
  bool Reduce(uint64_t new_size);

  // Adds |other| to |this| without assigning or releasing any reservation.
  // Used for seamless transition from one reservation to another (more generic
  // than std::move). Resets |other| to non-reserved state upon return from this
  // method.
  void HandOver(ScopedReservation& other);

 private:
  scoped_refptr<ResourceManager> resource_manager_;
  std::optional<uint64_t> size_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_RESOURCES_RESOURCE_MANAGER_H_
