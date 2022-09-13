// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fake_ack_handler.h"

#include "base/ranges/algorithm.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/public/ack_handle.h"
#include "components/invalidation/public/invalidation.h"

namespace invalidation {

namespace {

struct AckHandleMatcher {
  explicit AckHandleMatcher(const AckHandle& handle);
  bool operator()(const Invalidation& invalidation) const;

  AckHandle handle_;
};

AckHandleMatcher::AckHandleMatcher(const AckHandle& handle) : handle_(handle) {}

bool AckHandleMatcher::operator()(const Invalidation& invalidation) const {
  return handle_.Equals(invalidation.ack_handle());
}

}  // namespace

FakeAckHandler::FakeAckHandler() = default;

FakeAckHandler::~FakeAckHandler() = default;

void FakeAckHandler::RegisterInvalidation(Invalidation* invalidation) {
  unacked_invalidations_.push_back(*invalidation);
  invalidation->SetAckHandler(AsWeakPtr(), base::ThreadTaskRunnerHandle::Get());
}

void FakeAckHandler::RegisterUnsentInvalidation(Invalidation* invalidation) {
  unsent_invalidations_.push_back(*invalidation);
}

bool FakeAckHandler::IsUnacked(const Invalidation& invalidation) const {
  return base::ranges::find_if(unacked_invalidations_,
                               AckHandleMatcher(invalidation.ack_handle())) !=
         unacked_invalidations_.end();
}

bool FakeAckHandler::IsAcknowledged(const Invalidation& invalidation) const {
  return base::ranges::find_if(acked_invalidations_,
                               AckHandleMatcher(invalidation.ack_handle())) !=
         acked_invalidations_.end();
}

bool FakeAckHandler::IsDropped(const Invalidation& invalidation) const {
  return base::ranges::find_if(dropped_invalidations_,
                               AckHandleMatcher(invalidation.ack_handle())) !=
         dropped_invalidations_.end();
}

bool FakeAckHandler::IsUnsent(const Invalidation& invalidation) const {
  return base::ranges::find_if(unsent_invalidations_,
                               AckHandleMatcher(invalidation.ack_handle())) !=
         unsent_invalidations_.end();
}

bool FakeAckHandler::AllInvalidationsAccountedFor() const {
  return unacked_invalidations_.empty() && unrecovered_drop_events_.empty();
}

void FakeAckHandler::Acknowledge(const Topic& topic, const AckHandle& handle) {
  auto it =
      base::ranges::find_if(unacked_invalidations_, AckHandleMatcher(handle));
  if (it != unacked_invalidations_.end()) {
    acked_invalidations_.push_back(*it);
    unacked_invalidations_.erase(it);
  }

  auto it2 = unrecovered_drop_events_.find(topic);
  if (it2 != unrecovered_drop_events_.end() && it2->second.Equals(handle)) {
    unrecovered_drop_events_.erase(it2);
  }
}

void FakeAckHandler::Drop(const Topic& topic, const AckHandle& handle) {
  auto it =
      base::ranges::find_if(unacked_invalidations_, AckHandleMatcher(handle));
  if (it != unacked_invalidations_.end()) {
    dropped_invalidations_.push_back(*it);
    unacked_invalidations_.erase(it);
  }
  unrecovered_drop_events_.erase(topic);
  unrecovered_drop_events_.emplace(topic, handle);
}

}  // namespace invalidation
