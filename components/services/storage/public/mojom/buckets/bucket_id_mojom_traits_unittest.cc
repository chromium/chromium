// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/mojom/buckets/bucket_id_mojom_traits.h"

#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/mojom/buckets/bucket_id.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {
namespace {

TEST(BucketIdMojomTraitsTest, SerializeAndDeserialize) {
  BucketId test_keys[] = {
      BucketId(1),
      BucketId(123),
      BucketId(),
  };

  for (auto& original : test_keys) {
    BucketId copied;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::BucketId>(original, copied));
    EXPECT_EQ(original, copied);
  }
}

}  // namespace
}  // namespace storage
