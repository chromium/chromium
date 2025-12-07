// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/unique_ids.h"

#include "components/autofill/core/common/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace autofill {

namespace {

TEST(UniqueIdsTest, FieldGlobalIdWorksWithAbseilHashSet) {
  test::AutofillUnitTestEnvironment e;
  auto f1 = FieldGlobalId(e.NextLocalFrameToken(), FieldRendererId(1));
  auto f2 = FieldGlobalId(e.NextLocalFrameToken(), FieldRendererId(2));
  auto f3 = FieldGlobalId(f2.frame_token,
                          FieldRendererId(f2.renderer_id.value() + 1));

  absl::flat_hash_set<FieldGlobalId> set;
  EXPECT_FALSE(set.contains(f1));
  EXPECT_FALSE(set.contains(f2));
  EXPECT_FALSE(set.contains(f3));

  set.insert(f1);
  EXPECT_TRUE(set.contains(f1));
  EXPECT_FALSE(set.contains(f2));
  EXPECT_FALSE(set.contains(f3));

  set.insert(f2);
  EXPECT_TRUE(set.contains(f1));
  EXPECT_TRUE(set.contains(f2));
  EXPECT_FALSE(set.contains(f3));

  set.insert(f3);
  EXPECT_TRUE(set.contains(f1));
  EXPECT_TRUE(set.contains(f2));
  EXPECT_TRUE(set.contains(f3));
}

TEST(UniqueIdsTest, FormGlobalIdWorksWithAbseilHashSet) {
  test::AutofillUnitTestEnvironment e;
  auto f1 = FormGlobalId(e.NextLocalFrameToken(), FormRendererId(1));
  auto f2 = FormGlobalId(e.NextLocalFrameToken(), FormRendererId(2));
  auto f3 =
      FormGlobalId(f2.frame_token, FormRendererId(f2.renderer_id.value() + 1));

  absl::flat_hash_set<FormGlobalId> set;
  EXPECT_FALSE(set.contains(f1));
  EXPECT_FALSE(set.contains(f2));
  EXPECT_FALSE(set.contains(f3));

  set.insert(f1);
  EXPECT_TRUE(set.contains(f1));
  EXPECT_FALSE(set.contains(f2));
  EXPECT_FALSE(set.contains(f3));

  set.insert(f2);
  EXPECT_TRUE(set.contains(f1));
  EXPECT_TRUE(set.contains(f2));
  EXPECT_FALSE(set.contains(f3));

  set.insert(f3);
  EXPECT_TRUE(set.contains(f1));
  EXPECT_TRUE(set.contains(f2));
  EXPECT_TRUE(set.contains(f3));
}

}  // namespace

}  // namespace autofill
