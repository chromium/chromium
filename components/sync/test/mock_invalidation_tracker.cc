// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/mock_invalidation_tracker.h"

#include <memory>

#include "base/check_op.h"

namespace syncer {

std::unique_ptr<TrackableMockInvalidation>
MockInvalidationTracker::IssueUnknownVersionInvalidation() {
  return std::make_unique<TrackableMockInvalidation>(true, -1, std::string(),
                                                     this, next_id_++);
}

std::unique_ptr<TrackableMockInvalidation>
MockInvalidationTracker::IssueInvalidation(int64_t version,
                                           const std::string& payload) {
  return std::make_unique<TrackableMockInvalidation>(false, version, payload,
                                                     this, next_id_++);
}

MockInvalidationTracker::MockInvalidationTracker() = default;

MockInvalidationTracker::~MockInvalidationTracker() = default;

void MockInvalidationTracker::Acknowledge(int invalidation_id) {
  acknowledged_.insert(invalidation_id);
}

void MockInvalidationTracker::Drop(int invalidation_id) {
  dropped_.insert(invalidation_id);
}

bool MockInvalidationTracker::IsUnacked(int invalidation_id) const {
  DCHECK_LE(invalidation_id, next_id_);
  return !IsAcknowledged(invalidation_id) && !IsDropped(invalidation_id);
}

bool MockInvalidationTracker::IsAcknowledged(int invalidation_id) const {
  DCHECK_LE(invalidation_id, next_id_);
  return acknowledged_.find(invalidation_id) != acknowledged_.end();
}

bool MockInvalidationTracker::IsDropped(int invalidation_id) const {
  DCHECK_LE(invalidation_id, next_id_);
  return dropped_.find(invalidation_id) != dropped_.end();
}

bool MockInvalidationTracker::AllInvalidationsAccountedFor() const {
  for (int i = 0; i < next_id_; ++i) {
    if (IsUnacked(i)) {
      return false;
    }
  }
  return true;
}

}  // namespace syncer
