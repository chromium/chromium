// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/unacked_invalidation_set_test_util.h"

#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace syncer {

using ::testing::MakeMatcher;
using ::testing::MatchResultListener;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::PrintToString;

namespace test_util {

// This class needs to be declared outside the null namespace so the
// UnackedInvalidationSet can declare it as a friend.  This class needs access
// to the UnackedInvalidationSet internals to implement its comparispon
// function.
class UnackedInvalidationSetEqMatcher
    : public testing::MatcherInterface<const UnackedInvalidationSet&> {
 public:
  explicit UnackedInvalidationSetEqMatcher(
      const UnackedInvalidationSet& expected);

  bool MatchAndExplain(
      const UnackedInvalidationSet& actual,
      MatchResultListener* listener) const override;
  void DescribeTo(::std::ostream* os) const override;
  void DescribeNegationTo(::std::ostream* os) const override;

 private:
  const UnackedInvalidationSet expected_;

  DISALLOW_COPY_AND_ASSIGN(UnackedInvalidationSetEqMatcher);
};

namespace {

struct InvalidationEq {
  bool operator()(const syncer::Invalidation& a,
                  const syncer::Invalidation& b) const {
    return a.Equals(b);
  }
};

}  // namespace

UnackedInvalidationSetEqMatcher::UnackedInvalidationSetEqMatcher(
    const UnackedInvalidationSet& expected)
  : expected_(expected) {}

bool UnackedInvalidationSetEqMatcher::MatchAndExplain(
    const UnackedInvalidationSet& actual,
    MatchResultListener* listener) const {
  // Use our friendship with this class to compare the internals of two
  // instances.
  //
  // Note that the registration status is intentionally not considered
  // when performing this comparison.
  return expected_.object_id_ == actual.object_id_
      && std::equal(expected_.invalidations_.begin(),
                    expected_.invalidations_.end(),
                    actual.invalidations_.begin(),
                    InvalidationEq());
}

void UnackedInvalidationSetEqMatcher::DescribeTo(::std::ostream* os) const {
  *os << " is equal to " << PrintToString(expected_);
}

void UnackedInvalidationSetEqMatcher::DescribeNegationTo(
    ::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

// We're done declaring UnackedInvalidationSetEqMatcher.  Everything else can
// go into the null namespace.
namespace {

ObjectIdInvalidationMap UnackedInvalidationsMapToObjectIdInvalidationMap(
    const UnackedInvalidationsMap& state_map) {
  ObjectIdInvalidationMap object_id_invalidation_map;
  for (auto it = state_map.begin(); it != state_map.end(); ++it) {
    it->second.ExportInvalidations(
        base::WeakPtr<AckHandler>(),
        scoped_refptr<base::SingleThreadTaskRunner>(),
        &object_id_invalidation_map);
  }
  return object_id_invalidation_map;
}

class UnackedInvalidationsMapEqMatcher
    : public testing::MatcherInterface<const UnackedInvalidationsMap&> {
 public:
  explicit UnackedInvalidationsMapEqMatcher(
      const UnackedInvalidationsMap& expected);

  virtual bool MatchAndExplain(const UnackedInvalidationsMap& actual,
                               MatchResultListener* listener) const;
  virtual void DescribeTo(::std::ostream* os) const;
  virtual void DescribeNegationTo(::std::ostream* os) const;

 private:
  const UnackedInvalidationsMap expected_;

  DISALLOW_COPY_AND_ASSIGN(UnackedInvalidationsMapEqMatcher);
};

UnackedInvalidationsMapEqMatcher::UnackedInvalidationsMapEqMatcher(
    const UnackedInvalidationsMap& expected)
    : expected_(expected) {
}

bool UnackedInvalidationsMapEqMatcher::MatchAndExplain(
    const UnackedInvalidationsMap& actual,
    MatchResultListener* listener) const {
  ObjectIdInvalidationMap expected_inv =
      UnackedInvalidationsMapToObjectIdInvalidationMap(expected_);
  ObjectIdInvalidationMap actual_inv =
      UnackedInvalidationsMapToObjectIdInvalidationMap(actual);

  return expected_inv == actual_inv;
}

void UnackedInvalidationsMapEqMatcher::DescribeTo(
    ::std::ostream* os) const {
  *os << " is equal to " << PrintToString(expected_);
}

void UnackedInvalidationsMapEqMatcher::DescribeNegationTo(
    ::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

}  // namespace

void PrintTo(const UnackedInvalidationSet& invalidations,
             ::std::ostream* os) {
  std::unique_ptr<base::DictionaryValue> value = invalidations.ToValue();

  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  serializer.Serialize(*value);

  (*os) << output;
}

void PrintTo(const UnackedInvalidationsMap& map, ::std::ostream* os) {
  std::unique_ptr<base::ListValue> list(new base::ListValue);
  for (auto it = map.begin(); it != map.end(); ++it) {
    list->Append(it->second.ToValue());
  }

  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(true);
  serializer.Serialize(*list);

  (*os) << output;
}

Matcher<const UnackedInvalidationSet&> Eq(
    const UnackedInvalidationSet& expected) {
  return MakeMatcher(new UnackedInvalidationSetEqMatcher(expected));
}

Matcher<const UnackedInvalidationsMap&> Eq(
    const UnackedInvalidationsMap& expected) {
  return MakeMatcher(new UnackedInvalidationsMapEqMatcher(expected));
}

}  // namespace test_util

};
