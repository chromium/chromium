// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/model_type_test_util.h"

#include "base/macros.h"

namespace syncer {

void PrintTo(ModelTypeSet model_types, ::std::ostream* os) {
  *os << ModelTypeSetToString(model_types);
}

namespace {

// Matcher implementation for HasModelTypes().
class HasModelTypesMatcher : public ::testing::MatcherInterface<ModelTypeSet> {
 public:
  explicit HasModelTypesMatcher(ModelTypeSet expected_types)
      : expected_types_(expected_types) {}

  virtual ~HasModelTypesMatcher() {}

  virtual bool MatchAndExplain(ModelTypeSet model_types,
                               ::testing::MatchResultListener* listener) const {
    // No need to annotate listener since we already define PrintTo().
    return model_types == expected_types_;
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "has model types " << ModelTypeSetToString(expected_types_);
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "doesn't have model types " << ModelTypeSetToString(expected_types_);
  }

 private:
  const ModelTypeSet expected_types_;

  DISALLOW_COPY_AND_ASSIGN(HasModelTypesMatcher);
};

}  // namespace

::testing::Matcher<ModelTypeSet> HasModelTypes(ModelTypeSet expected_types) {
  return ::testing::MakeMatcher(new HasModelTypesMatcher(expected_types));
}

}  // namespace syncer
