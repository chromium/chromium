// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_UNACKED_INVALIDATION_SET_H_
#define COMPONENTS_INVALIDATION_IMPL_UNACKED_INVALIDATION_SET_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

class SingleTopicInvalidationSet;
class TopicInvalidationMap;
class AckHandle;
class UnackedInvalidationSet;

using UnackedInvalidationsMap = std::map<Topic, UnackedInvalidationSet>;

// Manages the set of invalidations that are awaiting local acknowledgement for
// a particular Topic.  This set of invalidations will be persisted across
// restarts, though this class is not directly responsible for that.
class INVALIDATION_EXPORT UnackedInvalidationSet {
 public:
  static const size_t kMaxBufferedInvalidations;

  explicit UnackedInvalidationSet(const Topic& topic);
  UnackedInvalidationSet(const UnackedInvalidationSet& other);
  ~UnackedInvalidationSet();

  // Returns the Topic of the invalidations this class is tracking.
  const Topic& topic() const;

  // Adds a new invalidation to the set awaiting acknowledgement.
  void Add(const Invalidation& invalidation);

  // Adds many new invalidations to the set awaiting acknowledgement.
  void AddSet(const SingleTopicInvalidationSet& invalidations);

  // Exports the set of invalidations awaiting acknowledgement as an
  // TopicInvalidationMap. Each of these invalidations will be associated
  // with the given |ack_handler|.
  //
  // The contents of the UnackedInvalidationSet are not directly modified by
  // this procedure, but the AckHandles stored in those exported invalidations
  // are likely to end up back here in calls to Acknowledge().
  void ExportInvalidations(
      base::WeakPtr<AckHandler> ack_handler,
      scoped_refptr<base::SingleThreadTaskRunner> ack_handler_task_runner,
      TopicInvalidationMap* out) const;

  // Given an AckHandle belonging to one of the contained invalidations, finds
  // the invalidation and drops it from the list.  It is considered to be
  // acknowledged, so there is no need to continue maintaining its state.
  void Acknowledge(const AckHandle& handle);

 private:

  typedef std::set<Invalidation, InvalidationVersionLessThan> InvalidationsSet;

  // Limits the list size to the given maximum.  This function will correctly
  // update this class' internal data to indicate if invalidations have been
  // dropped.
  void Truncate(size_t max_size);

  const Topic topic_;
  InvalidationsSet invalidations_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_UNACKED_INVALIDATION_SET_H_
