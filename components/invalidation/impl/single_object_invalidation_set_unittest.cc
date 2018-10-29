// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/single_object_invalidation_set.h"

#include <memory>

#include "components/invalidation/impl/invalidation_test_util.h"
#include "google/cacheinvalidation/types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class SingleObjectInvalidationSetTest : public testing::Test {
 public:
  SingleObjectInvalidationSetTest()
      : kId(ipc::invalidation::ObjectSource::TEST, "one") {
  }
 protected:
  const invalidation::ObjectId kId;
};

TEST_F(SingleObjectInvalidationSetTest, InsertionAndOrdering) {
  SingleObjectInvalidationSet l1;
  SingleObjectInvalidationSet l2;

  Invalidation inv0 = Invalidation::InitUnknownVersion(kId);
  Invalidation inv1 = Invalidation::Init(kId, 1, "one");
  Invalidation inv2 = Invalidation::Init(kId, 5, "five");

  l1.Insert(inv0);
  l1.Insert(inv1);
  l1.Insert(inv2);

  l2.Insert(inv1);
  l2.Insert(inv2);
  l2.Insert(inv0);

  ASSERT_EQ(3U, l1.GetSize());
  ASSERT_EQ(3U, l2.GetSize());

  auto it1 = l1.begin();
  auto it2 = l2.begin();
  EXPECT_THAT(inv0, Eq(*it1));
  EXPECT_THAT(inv0, Eq(*it2));
  it1++;
  it2++;
  EXPECT_THAT(inv1, Eq(*it1));
  EXPECT_THAT(inv1, Eq(*it2));
  it1++;
  it2++;
  EXPECT_THAT(inv2, Eq(*it1));
  EXPECT_THAT(inv2, Eq(*it2));
  it1++;
  it2++;
  EXPECT_TRUE(it1 == l1.end());
  EXPECT_TRUE(it2 == l2.end());
}

TEST_F(SingleObjectInvalidationSetTest, StartWithUnknownVersion) {
  SingleObjectInvalidationSet list;
  EXPECT_FALSE(list.StartsWithUnknownVersion());

  list.Insert(Invalidation::Init(kId, 1, "one"));
  EXPECT_FALSE(list.StartsWithUnknownVersion());

  list.Insert(Invalidation::InitUnknownVersion(kId));
  EXPECT_TRUE(list.StartsWithUnknownVersion());

  list.Clear();
  EXPECT_FALSE(list.StartsWithUnknownVersion());
}

TEST_F(SingleObjectInvalidationSetTest, SerializeEmpty) {
  SingleObjectInvalidationSet list;

  std::unique_ptr<base::ListValue> value = list.ToValue();
  ASSERT_TRUE(value.get());
  SingleObjectInvalidationSet deserialized;
  deserialized.ResetFromValue(*value);
  EXPECT_TRUE(list == deserialized);
}

TEST_F(SingleObjectInvalidationSetTest, SerializeOne) {
  SingleObjectInvalidationSet list;
  list.Insert(Invalidation::Init(kId, 1, "one"));

  std::unique_ptr<base::ListValue> value = list.ToValue();
  ASSERT_TRUE(value.get());
  SingleObjectInvalidationSet deserialized;
  deserialized.ResetFromValue(*value);
  EXPECT_TRUE(list == deserialized);
}

TEST_F(SingleObjectInvalidationSetTest, SerializeMany) {
  SingleObjectInvalidationSet list;
  list.Insert(Invalidation::Init(kId, 1, "one"));
  list.Insert(Invalidation::InitUnknownVersion(kId));

  std::unique_ptr<base::ListValue> value = list.ToValue();
  ASSERT_TRUE(value.get());
  SingleObjectInvalidationSet deserialized;
  deserialized.ResetFromValue(*value);
  EXPECT_TRUE(list == deserialized);
}

}  // namespace

}  // namespace syncer
