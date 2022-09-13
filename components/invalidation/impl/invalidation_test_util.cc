// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_test_util.h"

#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/values.h"
#include "components/invalidation/public/invalidation.h"

namespace invalidation {

using ::testing::MakeMatcher;
using ::testing::MatchResultListener;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::PrintToString;

namespace {

class AckHandleEqMatcher : public MatcherInterface<const AckHandle&> {
 public:
  explicit AckHandleEqMatcher(const AckHandle& expected);

  bool MatchAndExplain(const AckHandle& actual,
                       MatchResultListener* listener) const override;
  void DescribeTo(::std::ostream* os) const override;
  void DescribeNegationTo(::std::ostream* os) const override;

 private:
  const AckHandle expected_;
};

AckHandleEqMatcher::AckHandleEqMatcher(const AckHandle& expected)
    : expected_(expected) {
}

bool AckHandleEqMatcher::MatchAndExplain(const AckHandle& actual,
                                         MatchResultListener* listener) const {
  return expected_.Equals(actual);
}

void AckHandleEqMatcher::DescribeTo(::std::ostream* os) const {
  *os << " is equal to " << PrintToString(expected_);
}

void AckHandleEqMatcher::DescribeNegationTo(::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

class InvalidationEqMatcher : public MatcherInterface<const Invalidation&> {
 public:
  explicit InvalidationEqMatcher(const Invalidation& expected);

  bool MatchAndExplain(const Invalidation& actual,
                       MatchResultListener* listener) const override;
  void DescribeTo(::std::ostream* os) const override;
  void DescribeNegationTo(::std::ostream* os) const override;

 private:
  const Invalidation expected_;
};

InvalidationEqMatcher::InvalidationEqMatcher(const Invalidation& expected)
    : expected_(expected) {
}

bool InvalidationEqMatcher::MatchAndExplain(
    const Invalidation& actual,
    MatchResultListener* listener) const {
  if (expected_.topic() != actual.topic())
    return false;
  if (expected_.is_unknown_version() && actual.is_unknown_version())
    return true;
  if (expected_.is_unknown_version() != actual.is_unknown_version())
    return false;
  // Neither is unknown version.
  return expected_.payload() == actual.payload() &&
         expected_.version() == actual.version();
}

void InvalidationEqMatcher::DescribeTo(::std::ostream* os) const {
  *os << " is equal to " << PrintToString(expected_);
}

void InvalidationEqMatcher::DescribeNegationTo(::std::ostream* os) const {
  *os << " isn't equal to " << PrintToString(expected_);
}

}  // namespace

void PrintTo(const AckHandle& ack_handle, ::std::ostream* os) {
  std::string printable_ack_handle;
  base::JSONWriter::Write(ack_handle.ToValue(), &printable_ack_handle);
  *os << "{ ack_handle: " << printable_ack_handle << " }";
}

Matcher<const AckHandle&> Eq(const AckHandle& expected) {
  return MakeMatcher(new AckHandleEqMatcher(expected));
}

void PrintTo(const Invalidation& inv, ::std::ostream* os) {
  *os << "{ payload: " << inv.ToString() << " }";
}

Matcher<const Invalidation&> Eq(const Invalidation& expected) {
  return MakeMatcher(new InvalidationEqMatcher(expected));
}

}  // namespace invalidation
