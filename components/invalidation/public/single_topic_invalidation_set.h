// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_SINGLE_TOPIC_INVALIDATION_SET_H_
#define COMPONENTS_INVALIDATION_PUBLIC_SINGLE_TOPIC_INVALIDATION_SET_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "components/invalidation/public/invalidation.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

// Holds a list of invalidations that all share the same Topic.
//
// The list is kept sorted by version to make it easier to perform common
// operations, like checking for an unknown version invalidation or fetching the
// highest invalidation with the highest version number.
class INVALIDATION_EXPORT SingleTopicInvalidationSet {
 public:
  typedef std::set<Invalidation, InvalidationVersionLessThan> InvalidationsSet;
  typedef InvalidationsSet::const_iterator const_iterator;
  typedef InvalidationsSet::const_reverse_iterator const_reverse_iterator;

  SingleTopicInvalidationSet();
  SingleTopicInvalidationSet(const SingleTopicInvalidationSet& other);
  SingleTopicInvalidationSet& operator=(
      const SingleTopicInvalidationSet& other);
  ~SingleTopicInvalidationSet();

  void Insert(const Invalidation& invalidation);
  void InsertAll(const SingleTopicInvalidationSet& other);
  void Clear();
  void Erase(const_iterator it);

  // Returns true if this list contains an unknown version.
  //
  // Unknown version invalidations always end up at the start of the list,
  // because they have the lowest possible value in the sort ordering.
  bool StartsWithUnknownVersion() const;
  size_t GetSize() const;
  bool IsEmpty() const;
  bool operator==(const SingleTopicInvalidationSet& other) const;

  const_iterator begin() const;
  const_iterator end() const;
  const_reverse_iterator rbegin() const;
  const_reverse_iterator rend() const;
  const Invalidation& back() const;

 private:
  InvalidationsSet invalidations_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_SINGLE_TOPIC_INVALIDATION_SET_H_
