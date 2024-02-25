// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MOCK_INVALIDATION_TRACKER_H_
#define COMPONENTS_SYNC_TEST_MOCK_INVALIDATION_TRACKER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "components/sync/test/trackable_mock_invalidation.h"

namespace syncer {

// Instantiates and track the acknowledgement state of
// TrackableMockInvalidations.
class MockInvalidationTracker {
 public:
  // Builers to return new TrackableMockInvalidations associated with this
  // object.
  std::unique_ptr<TrackableMockInvalidation> IssueUnknownVersionInvalidation();
  std::unique_ptr<TrackableMockInvalidation> IssueInvalidation(
      int64_t version,
      const std::string& payload);

  MockInvalidationTracker();
  ~MockInvalidationTracker();

  // Records the acknowledgement of the invalidation
  // specified by the given ID.
  void Acknowledge(int invaliation_id);

  // Records the drop of the invalidation specified by the given ID.
  void Drop(int invalidation_id);

  // Returns true if the invalidation associated with the given ID is neither
  // acknowledged nor dropped.
  bool IsUnacked(int invalidation_id) const;

  // Returns true if the invalidation associated with the given ID is
  // acknowledged.
  bool IsAcknowledged(int invalidation_id) const;

  // Returns true if the invalidation associated with the given ID is dropped.
  bool IsDropped(int invalidation_id) const;

  // Returns true if all issued invalidations were acknowledged or dropped.
  bool AllInvalidationsAccountedFor() const;

 private:
  // A counter used to assign strictly increasing IDs to each invalidation
  // issued by this class.
  int next_id_ = 0;

  // Acknowledgements and drops are tracked by adding the IDs for the
  // acknowledged or dropped items to the proper set.  An invalidation may be
  // both dropped and acknowledged if it represents the recovery from a drop
  // event.
  std::set<int> dropped_;
  std::set<int> acknowledged_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MOCK_INVALIDATION_TRACKER_H_
