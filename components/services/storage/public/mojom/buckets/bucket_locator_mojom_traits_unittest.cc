// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/mojom/buckets/bucket_locator_mojom_traits.h"

#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/mojom/buckets/bucket_locator.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace storage {
namespace {

TEST(BucketLocatorMojomTraitsTest, SerializeAndDeserialize) {
  BucketLocator test_keys[] = {
      BucketLocator(
          BucketId(1),
          blink::StorageKey::CreateFromStringForTesting("http://example/"),
          blink::mojom::StorageType::kTemporary, /*is_default=*/false),
      BucketLocator(
          BucketId(123),
          blink::StorageKey::CreateFromStringForTesting("http://google.com/"),
          blink::mojom::StorageType::kTemporary, /*is_default=*/true),
      BucketLocator(
          BucketId(1000),
          blink::StorageKey::CreateFromStringForTesting("http://test.com/"),
          blink::mojom::StorageType::kSyncable, /*is_default=*/true)};

  for (auto& original : test_keys) {
    BucketLocator copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BucketLocator>(
        original, copied));
    EXPECT_EQ(original, copied);
  }
}

}  // namespace
}  // namespace storage
