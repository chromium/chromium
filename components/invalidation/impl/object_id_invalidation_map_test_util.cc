// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/invalidation/impl/object_id_invalidation_map_test_util.h"

#include <algorithm>


namespace syncer {

using ::testing::MakeMatcher;
using ::testing::MatchResultListener;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::PrintToString;

namespace {

class ObjectIdInvalidationMapEqMatcher
    : public MatcherInterface<const ObjectIdInvalidationMap&> {
 public:
  explicit ObjectIdInvalidationMapEqMatcher(
      const ObjectIdInvalidationMap& expected);

  virtual bool MatchAndExplain(const ObjectIdInvalidationMap& lhs,
                               MatchResultListener* listener) const;
  virtual void DescribeTo(::std::ostream* os) const;
  virtual void DescribeNegationTo(::std::ostream* os) const;

 private:
  const ObjectIdInvalidationMap expected_;

  DISALLOW_COPY_AND_ASSIGN(ObjectIdInvalidationMapEqMatcher);
};

ObjectIdInvalidationMapEqMatcher::ObjectIdInvalidationMapEqMatcher(
    const ObjectIdInvalidationMap& expected) : expected_(expected) {
}

struct InvalidationEqPredicate {
  InvalidationEqPredicate(const Invalidation& inv1)
      : inv1_(inv1) { }

  bool operator()(const Invalidation& inv2) {
    return inv1_.Equals(inv2);
  }

  const Invalidation& inv1_;
};

bool ObjectIdInvalidationMapEqMatcher::MatchAndExplain(
    const ObjectIdInvalidationMap& actual,
    MatchResultListener* listener) const {

  std::vector<syncer::Invalidation> expected_invalidations;
  std::vector<syncer::Invalidation> actual_invalidations;

  expected_.GetAllInvalidations(&expected_invalidations);
  actual.GetAllInvalidations(&actual_invalidations);

  std::vector<syncer::Invalidation> expected_only;
  std::vector<syncer::Invalidation> actual_only;

  for (auto it = expected_invalidations.begin();
       it != expected_invalidations.end(); ++it) {
    if (std::find_if(actual_invalidations.begin(),
                     actual_invalidations.end(),
                     InvalidationEqPredicate(*it))
        == actual_invalidations.end()) {
      expected_only.push_back(*it);
    }
  }

  for (auto it = actual_invalidations.begin(); it != actual_invalidations.end();
       ++it) {
    if (std::find_if(expected_invalidations.begin(),
                     expected_invalidations.end(),
                     InvalidationEqPredicate(*it))
        == expected_invalidations.end()) {
      actual_only.push_back(*it);
    }
  }

  if (expected_only.empty() && actual_only.empty())
    return true;

  bool printed_header = false;
  if (!actual_only.empty()) {
    *listener << " which has these unexpected elements: "
              << PrintToString(actual_only);
    printed_header = true;
  }

  if (!expected_only.empty()) {
    *listener << (printed_header ? ",\nand" : "which")
              << " doesn't have these expected elements: "
              << PrintToString(expected_only);
    printed_header = true;
  }

  return false;
}

void ObjectIdInvalidationMapEqMatcher::DescribeTo(::std::ostream* os) const {
  *os << " is equal to " << PrintToString(expected_);
}

void ObjectIdInvalidationMapEqMatcher::DescribeNegationTo(
    ::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

}  // namespace

Matcher<const ObjectIdInvalidationMap&> Eq(
    const ObjectIdInvalidationMap& expected) {
  return MakeMatcher(new ObjectIdInvalidationMapEqMatcher(expected));
}

}  // namespace syncer
