// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/data_type_test_util.h"

namespace syncer {

void PrintTo(DataTypeSet data_types, ::std::ostream* os) {
  *os << DataTypeSetToDebugString(data_types);
}

namespace {

// Matcher implementation for HasDataTypes().
class HasDataTypesMatcher : public ::testing::MatcherInterface<DataTypeSet> {
 public:
  explicit HasDataTypesMatcher(DataTypeSet expected_types)
      : expected_types_(expected_types) {}

  HasDataTypesMatcher(const HasDataTypesMatcher&) = delete;
  HasDataTypesMatcher& operator=(const HasDataTypesMatcher&) = delete;

  ~HasDataTypesMatcher() override = default;

  bool MatchAndExplain(
      DataTypeSet data_types,
      ::testing::MatchResultListener* listener) const override {
    // No need to annotate listener since we already define PrintTo().
    return data_types == expected_types_;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "has data types " << DataTypeSetToDebugString(expected_types_);
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "doesn't have data types "
        << DataTypeSetToDebugString(expected_types_);
  }

 private:
  const DataTypeSet expected_types_;
};

}  // namespace

::testing::Matcher<DataTypeSet> HasDataTypes(DataTypeSet expected_types) {
  return ::testing::MakeMatcher(new HasDataTypesMatcher(expected_types));
}

}  // namespace syncer
