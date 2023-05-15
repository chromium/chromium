// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/mojom/buckets/bucket_info_mojom_traits.h"

#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/mojom/buckets/bucket_info.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {
namespace {

TEST(BucketInfoMojomTraitsTest, SerializeAndDeserialize) {
  BucketInfo test_keys[] = {
      BucketInfo(
          BucketId(1),
          blink::StorageKey::CreateFromStringForTesting("http://example/"),
          blink::mojom::StorageType::kTemporary, kDefaultBucketName,
          base::Time(), 0, true, blink::mojom::BucketDurability::kRelaxed),
      BucketInfo(
          BucketId(123),
          blink::StorageKey::CreateFromStringForTesting("http://google.com/"),
          blink::mojom::StorageType::kTemporary, "inbox",
          base::Time::Now() + base::Days(1), 100, true,
          blink::mojom::BucketDurability::kStrict),
      BucketInfo(
          BucketId(1000),
          blink::StorageKey::CreateFromStringForTesting("http://test.com/"),
          blink::mojom::StorageType::kTemporary, "drafts",
          base::Time::Now() - base::Days(1), 1234, false,
          blink::mojom::BucketDurability::kStrict),
  };

  for (auto& original : test_keys) {
    BucketInfo copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BucketInfo>(original,
                                                                       copied));
    EXPECT_EQ(original, copied);
  }
}

}  // namespace
}  // namespace storage
