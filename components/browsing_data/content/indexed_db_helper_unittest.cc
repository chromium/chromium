// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/indexed_db_helper.h"

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace browsing_data {
namespace {

class CannedIndexedDBHelperTest : public testing::Test {
 public:
  content::StoragePartition* StoragePartition() {
    return browser_context_.GetDefaultStoragePartition();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

TEST_F(CannedIndexedDBHelperTest, Empty) {
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFromStringForTesting("http://host1:1/");
  auto helper = base::MakeRefCounted<CannedIndexedDBHelper>(StoragePartition());

  ASSERT_TRUE(helper->empty());
  helper->Add(storage_key);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedIndexedDBHelperTest, Delete) {
  const blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting("http://host1:9000");
  const blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting("http://example.com");

  auto helper = base::MakeRefCounted<CannedIndexedDBHelper>(StoragePartition());

  EXPECT_TRUE(helper->empty());
  helper->Add(storage_key1);
  helper->Add(storage_key2);
  EXPECT_EQ(2u, helper->GetCount());
  base::RunLoop loop;
  helper->DeleteIndexedDB(storage_key2,
                          base::BindLambdaForTesting([&](bool success) {
                            EXPECT_TRUE(success);
                            loop.Quit();
                          }));
  loop.Run();
  EXPECT_EQ(1u, helper->GetCount());
}

TEST_F(CannedIndexedDBHelperTest, IgnoreExtensionsAndDevTools) {
  const blink::StorageKey storage_key1 =
      blink::StorageKey::CreateFromStringForTesting(
          "chrome-extension://abcdefghijklmnopqrstuvwxyz/");
  const blink::StorageKey storage_key2 =
      blink::StorageKey::CreateFromStringForTesting(
          "devtools://abcdefghijklmnopqrstuvwxyz/");

  auto helper = base::MakeRefCounted<CannedIndexedDBHelper>(StoragePartition());

  ASSERT_TRUE(helper->empty());
  helper->Add(storage_key1);
  ASSERT_TRUE(helper->empty());
  helper->Add(storage_key2);
  ASSERT_TRUE(helper->empty());
}

}  // namespace
}  // namespace browsing_data
