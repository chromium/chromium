// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/topic_invalidation_map_test_util.h"

#include <base/containers/contains.h>
#include <algorithm>

namespace invalidation {

using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::PrintToString;

namespace {

class TopicInvalidationMapEqMatcher
    : public MatcherInterface<const TopicInvalidationMap&> {
 public:
  explicit TopicInvalidationMapEqMatcher(const TopicInvalidationMap& expected);
  ~TopicInvalidationMapEqMatcher() override = default;

  bool MatchAndExplain(const TopicInvalidationMap& lhs,
                       MatchResultListener* listener) const override;
  void DescribeTo(::std::ostream* os) const override;
  void DescribeNegationTo(::std::ostream* os) const override;

 private:
  const TopicInvalidationMap expected_;
};

TopicInvalidationMapEqMatcher::TopicInvalidationMapEqMatcher(
    const TopicInvalidationMap& expected)
    : expected_(expected) {}

bool TopicInvalidationMapEqMatcher::MatchAndExplain(
    const TopicInvalidationMap& actual,
    MatchResultListener* listener) const {
  std::vector<Invalidation> expected_invalidations;
  std::vector<Invalidation> actual_invalidations;

  expected_.GetAllInvalidations(&expected_invalidations);
  actual.GetAllInvalidations(&actual_invalidations);

  std::vector<Invalidation> expected_only;
  std::vector<Invalidation> actual_only;

  for (const auto& expected_invalidation : expected_invalidations) {
    if (!base::Contains(actual_invalidations, expected_invalidation)) {
      expected_only.push_back(expected_invalidation);
    }
  }

  for (const auto& actual_invalidation : actual_invalidations) {
    if (!base::Contains(expected_invalidations, actual_invalidation)) {
      actual_only.push_back(actual_invalidation);
    }
  }

  if (expected_only.empty() && actual_only.empty()) {
    return true;
  }

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

void TopicInvalidationMapEqMatcher::DescribeTo(::std::ostream* os) const {
  // TODO(crbug.com/1055286): seems there is no custom printer for
  // TopicInvalidationMap.
  *os << " is equal to " << PrintToString(expected_);
}

void TopicInvalidationMapEqMatcher::DescribeNegationTo(
    ::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

}  // namespace

Matcher<const TopicInvalidationMap&> Eq(const TopicInvalidationMap& expected) {
  return MakeMatcher(new TopicInvalidationMapEqMatcher(expected));
}

}  // namespace invalidation
