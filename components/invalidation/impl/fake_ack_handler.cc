// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fake_ack_handler.h"

#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
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
  invalidation->SetAckHandler(
      AsWeakPtr(), base::SingleThreadTaskRunner::GetCurrentDefault());
}

void FakeAckHandler::RegisterUnsentInvalidation(Invalidation* invalidation) {
  unsent_invalidations_.push_back(*invalidation);
}

bool FakeAckHandler::IsUnacked(const Invalidation& invalidation) const {
  return base::ranges::any_of(unacked_invalidations_,
                              AckHandleMatcher(invalidation.ack_handle()));
}

bool FakeAckHandler::IsAcknowledged(const Invalidation& invalidation) const {
  return base::ranges::any_of(acked_invalidations_,
                              AckHandleMatcher(invalidation.ack_handle()));
}

bool FakeAckHandler::IsUnsent(const Invalidation& invalidation) const {
  return base::ranges::any_of(unsent_invalidations_,
                              AckHandleMatcher(invalidation.ack_handle()));
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

}  // namespace invalidation
