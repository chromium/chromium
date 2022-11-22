// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/single_topic_invalidation_set.h"

#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

SingleTopicInvalidationSet::SingleTopicInvalidationSet() = default;

SingleTopicInvalidationSet::SingleTopicInvalidationSet(
    const SingleTopicInvalidationSet& other) = default;

SingleTopicInvalidationSet& SingleTopicInvalidationSet::operator=(
    const SingleTopicInvalidationSet& other) = default;

SingleTopicInvalidationSet::~SingleTopicInvalidationSet() = default;

void SingleTopicInvalidationSet::Insert(const Invalidation& invalidation) {
  invalidations_.insert(invalidation);
}

void SingleTopicInvalidationSet::InsertAll(
    const SingleTopicInvalidationSet& other) {
  invalidations_.insert(other.begin(), other.end());
}

void SingleTopicInvalidationSet::Clear() {
  invalidations_.clear();
}

void SingleTopicInvalidationSet::Erase(const_iterator it) {
  invalidations_.erase(*it);
}

bool SingleTopicInvalidationSet::StartsWithUnknownVersion() const {
  return !invalidations_.empty() &&
         invalidations_.begin()->is_unknown_version();
}

size_t SingleTopicInvalidationSet::GetSize() const {
  return invalidations_.size();
}

bool SingleTopicInvalidationSet::IsEmpty() const {
  return invalidations_.empty();
}

bool SingleTopicInvalidationSet::operator==(
    const SingleTopicInvalidationSet& other) const {
  return invalidations_ == other.invalidations_;
}

SingleTopicInvalidationSet::const_iterator SingleTopicInvalidationSet::begin()
    const {
  return invalidations_.begin();
}

SingleTopicInvalidationSet::const_iterator SingleTopicInvalidationSet::end()
    const {
  return invalidations_.end();
}

SingleTopicInvalidationSet::const_reverse_iterator
SingleTopicInvalidationSet::rbegin() const {
  return invalidations_.rbegin();
}

SingleTopicInvalidationSet::const_reverse_iterator
SingleTopicInvalidationSet::rend() const {
  return invalidations_.rend();
}

const Invalidation& SingleTopicInvalidationSet::back() const {
  return *invalidations_.rbegin();
}

}  // namespace invalidation
