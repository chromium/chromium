// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/local_storage_helper.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace browsing_data {

namespace {

class CannedLocalStorageTest : public testing::Test {
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(CannedLocalStorageTest, Empty) {
  content::TestBrowserContext context;

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");

  auto helper = base::MakeRefCounted<CannedLocalStorageHelper>(
      context.GetDefaultStoragePartition());

  ASSERT_TRUE(helper->empty());
  helper->Add(storage_key);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedLocalStorageTest, Delete) {
  content::TestBrowserContext context;

  const blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://host1:9000");
  const blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");
  const blink::StorageKey storage_key3 =
      blink::StorageKey::CreateFromStringForTesting("http://foo.example.com");

  auto helper = base::MakeRefCounted<CannedLocalStorageHelper>(
      context.GetDefaultStoragePartition());

  EXPECT_TRUE(helper->empty());
  helper->Add(storage_key1);
  helper->Add(storage_key2);
  helper->Add(storage_key3);
  EXPECT_EQ(3u, helper->GetCount());
  helper->DeleteStorageKey(storage_key2, base::DoNothing());
  EXPECT_EQ(2u, helper->GetCount());
  helper->DeleteStorageKey(storage_key1, base::DoNothing());
  EXPECT_EQ(1u, helper->GetCount());
}

TEST_F(CannedLocalStorageTest, IgnoreExtensionsAndDevTools) {
  content::TestBrowserContext context;

  const blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting(
          "chrome-extension://abcdefghijklmnopqrstuvwxyz/");
  const blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting(
          "devtools://abcdefghijklmnopqrstuvwxyz/");

  auto helper = base::MakeRefCounted<CannedLocalStorageHelper>(
      context.GetDefaultStoragePartition());

  ASSERT_TRUE(helper->empty());
  helper->Add(storage_key1);
  ASSERT_TRUE(helper->empty());
  helper->Add(storage_key2);
  ASSERT_TRUE(helper->empty());
}

}  // namespace

}  // namespace browsing_data
