// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/trackable_mock_invalidation.h"

#include "components/sync/test/mock_invalidation_tracker.h"

namespace syncer {

TrackableMockInvalidation::TrackableMockInvalidation(
    bool is_unknown_version,
    int64_t version,
    const std::string& payload,
    MockInvalidationTracker* tracker,
    int tracking_id)
    : MockInvalidation(is_unknown_version, version, payload),
      tracker_(tracker),
      tracking_id_(tracking_id) {}

TrackableMockInvalidation::~TrackableMockInvalidation() = default;

void TrackableMockInvalidation::Acknowledge() {
  if (tracker_) {
    tracker_->Acknowledge(tracking_id_);
  }
}

void TrackableMockInvalidation::Drop() {
  if (tracker_) {
    tracker_->Drop(tracking_id_);
  }
}

int TrackableMockInvalidation::GetTrackingId() {
  return tracking_id_;
}

}  // namespace syncer
