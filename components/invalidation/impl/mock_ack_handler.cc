// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/mock_ack_handler.h"

#include <algorithm>

#include "base/threading/thread_task_runner_handle.h"
#include "components/invalidation/public/ack_handle.h"
#include "components/invalidation/public/invalidation.h"

namespace syncer {

namespace {

struct AckHandleMatcher {
  AckHandleMatcher(const AckHandle& handle);
  bool operator()(const syncer::Invalidation& invalidation) const;

  syncer::AckHandle handle_;
};

AckHandleMatcher::AckHandleMatcher(const AckHandle& handle)
  : handle_(handle) {}

bool AckHandleMatcher::operator()(
    const syncer::Invalidation& invalidation) const {
  return handle_.Equals(invalidation.ack_handle());
}

}  // namespace

MockAckHandler::MockAckHandler() {}

MockAckHandler::~MockAckHandler() {}

void MockAckHandler::RegisterInvalidation(Invalidation* invalidation) {
  unacked_invalidations_.push_back(*invalidation);
  invalidation->SetAckHandler(AsWeakPtr(), base::ThreadTaskRunnerHandle::Get());
}

void MockAckHandler::RegisterUnsentInvalidation(Invalidation* invalidation) {
  unsent_invalidations_.push_back(*invalidation);
}

bool MockAckHandler::IsUnacked(const Invalidation& invalidation) const {
  AckHandleMatcher matcher(invalidation.ack_handle());
  auto it = std::find_if(unacked_invalidations_.begin(),
                         unacked_invalidations_.end(), matcher);
  return it != unacked_invalidations_.end();
}

bool MockAckHandler::IsAcknowledged(const Invalidation& invalidation) const {
  AckHandleMatcher matcher(invalidation.ack_handle());
  auto it = std::find_if(acked_invalidations_.begin(),
                         acked_invalidations_.end(), matcher);
  return it != acked_invalidations_.end();
}

bool MockAckHandler::IsDropped(const Invalidation& invalidation) const {
  AckHandleMatcher matcher(invalidation.ack_handle());
  auto it = std::find_if(dropped_invalidations_.begin(),
                         dropped_invalidations_.end(), matcher);
  return it != dropped_invalidations_.end();
}

bool MockAckHandler::IsUnsent(const Invalidation& invalidation) const {
  AckHandleMatcher matcher(invalidation.ack_handle());
  auto it1 = std::find_if(unsent_invalidations_.begin(),
                          unsent_invalidations_.end(), matcher);
  return it1 != unsent_invalidations_.end();
}

bool MockAckHandler::AllInvalidationsAccountedFor() const {
  return unacked_invalidations_.empty() && unrecovered_drop_events_.empty();
}

void MockAckHandler::Acknowledge(
    const invalidation::ObjectId& id,
    const AckHandle& handle) {
  AckHandleMatcher matcher(handle);
  auto it = std::find_if(unacked_invalidations_.begin(),
                         unacked_invalidations_.end(), matcher);
  if (it != unacked_invalidations_.end()) {
    acked_invalidations_.push_back(*it);
    unacked_invalidations_.erase(it);
  }

  auto it2 = unrecovered_drop_events_.find(id);
  if (it2 != unrecovered_drop_events_.end() && it2->second.Equals(handle)) {
    unrecovered_drop_events_.erase(it2);
  }
}

void MockAckHandler::Drop(
    const invalidation::ObjectId& id,
    const AckHandle& handle) {
  AckHandleMatcher matcher(handle);
  auto it = std::find_if(unacked_invalidations_.begin(),
                         unacked_invalidations_.end(), matcher);
  if (it != unacked_invalidations_.end()) {
    dropped_invalidations_.push_back(*it);
    unacked_invalidations_.erase(it);
  }
  unrecovered_drop_events_.erase(id);
  unrecovered_drop_events_.insert(std::make_pair(id, handle));
}

}  // namespace syncer
