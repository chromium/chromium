// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/model_type_test_util.h"

namespace syncer {

void PrintTo(ModelTypeSet model_types, ::std::ostream* os) {
  *os << ModelTypeSetToDebugString(model_types);
}

namespace {

// Matcher implementation for HasModelTypes().
class HasModelTypesMatcher : public ::testing::MatcherInterface<ModelTypeSet> {
 public:
  explicit HasModelTypesMatcher(ModelTypeSet expected_types)
      : expected_types_(expected_types) {}

  HasModelTypesMatcher(const HasModelTypesMatcher&) = delete;
  HasModelTypesMatcher& operator=(const HasModelTypesMatcher&) = delete;

  ~HasModelTypesMatcher() override = default;

  bool MatchAndExplain(
      ModelTypeSet model_types,
      ::testing::MatchResultListener* listener) const override {
    // No need to annotate listener since we already define PrintTo().
    return model_types == expected_types_;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "has model types " << ModelTypeSetToDebugString(expected_types_);
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "doesn't have model types "
        << ModelTypeSetToDebugString(expected_types_);
  }

 private:
  const ModelTypeSet expected_types_;
};

}  // namespace

::testing::Matcher<ModelTypeSet> HasModelTypes(ModelTypeSet expected_types) {
  return ::testing::MakeMatcher(new HasModelTypesMatcher(expected_types));
}

}  // namespace syncer
