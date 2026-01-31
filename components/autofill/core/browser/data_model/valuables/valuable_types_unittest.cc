// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

TEST(ValuableTypesTest, VerifyMembers) {
  ValuableId id("123");
  base::Time use_date = base::Time::Now();
  int64_t use_count = 5;

  ValuableMetadata metadata(id, use_date, use_count);

  EXPECT_EQ(metadata, ValuableMetadata(id, use_date, use_count));
}

TEST(ValuableTypesTest, Equality) {
  ValuableId id("123");
  base::Time use_date = base::Time::Now();
  int64_t use_count = 5;

  ValuableMetadata metadata(id, use_date, use_count);
  ValuableMetadata metadata_copy(id, use_date, use_count);
  ValuableMetadata metadata_diff_id(ValuableId("456"), use_date, use_count);
  ValuableMetadata metadata_diff_date(id, use_date + base::Seconds(1),
                                      use_count);
  ValuableMetadata metadata_diff_count(id, use_date, use_count + 1);

  EXPECT_EQ(metadata, metadata_copy);
  EXPECT_NE(metadata, metadata_diff_id);
  EXPECT_NE(metadata, metadata_diff_date);
  EXPECT_NE(metadata, metadata_diff_count);
}

}  // namespace

}  // namespace autofill
