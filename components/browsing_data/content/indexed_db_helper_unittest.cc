// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/indexed_db_helper.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browsing_data {
namespace {

class CannedIndexedDBHelperTest : public testing::Test {
 public:
  content::StoragePartition* StoragePartition() {
    return content::BrowserContext::GetDefaultStoragePartition(
        &browser_context_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

TEST_F(CannedIndexedDBHelperTest, Empty) {
  const url::Origin origin = url::Origin::Create(GURL("http://host1:1/"));
  scoped_refptr<CannedIndexedDBHelper> helper(
      new CannedIndexedDBHelper(StoragePartition()));

  ASSERT_TRUE(helper->empty());
  helper->Add(origin);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedIndexedDBHelperTest, Delete) {
  const url::Origin origin1 = url::Origin::Create(GURL("http://host1:9000"));
  const url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));

  scoped_refptr<CannedIndexedDBHelper> helper(
      new CannedIndexedDBHelper(StoragePartition()));

  EXPECT_TRUE(helper->empty());
  helper->Add(origin1);
  helper->Add(origin2);
  EXPECT_EQ(2u, helper->GetCount());
  base::RunLoop loop;
  helper->DeleteIndexedDB(origin2,
                          base::BindLambdaForTesting([&](bool success) {
                            EXPECT_TRUE(success);
                            loop.Quit();
                          }));
  loop.Run();
  EXPECT_EQ(1u, helper->GetCount());
}

TEST_F(CannedIndexedDBHelperTest, IgnoreExtensionsAndDevTools) {
  const url::Origin origin1 = url::Origin::Create(
      GURL("chrome-extension://abcdefghijklmnopqrstuvwxyz/"));
  const url::Origin origin2 =
      url::Origin::Create(GURL("devtools://abcdefghijklmnopqrstuvwxyz/"));

  scoped_refptr<CannedIndexedDBHelper> helper(
      new CannedIndexedDBHelper(StoragePartition()));

  ASSERT_TRUE(helper->empty());
  helper->Add(origin1);
  ASSERT_TRUE(helper->empty());
  helper->Add(origin2);
  ASSERT_TRUE(helper->empty());
}

}  // namespace
}  // namespace browsing_data
