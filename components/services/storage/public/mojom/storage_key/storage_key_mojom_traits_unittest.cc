// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/mojom/storage_key/storage_key_mojom_traits.h"

#include "components/services/storage/public/cpp/storage_key.h"
#include "components/services/storage/public/mojom/storage_key/storage_key.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace storage {
namespace {

TEST(StorageKeyMojomTraitsTest, SerializeAndDeserialize) {
  StorageKey test_keys[] = {
      StorageKey(url::Origin::Create(GURL("https://example.com"))),
      StorageKey(url::Origin::Create(GURL("http://example.com"))),
      StorageKey(url::Origin::Create(GURL("https://example.test"))),
      StorageKey(url::Origin::Create(GURL("https://sub.example.com"))),
      StorageKey(url::Origin::Create(GURL("http://sub2.example.com"))),
      StorageKey(url::Origin()),
  };

  for (auto& original : test_keys) {
    StorageKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::StorageKey>(original,
                                                                       copied));
    EXPECT_EQ(original, copied);
  }
}

}  // namespace
}  // namespace storage