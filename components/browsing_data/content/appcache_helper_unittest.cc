// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/appcache_helper.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

class CannedAppCacheHelperTest : public testing::Test {
 public:
  CannedAppCacheHelperTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  void TearDown() override {
    // Make sure we run all pending tasks on IO thread before testing
    // browser context is destructed.
    content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
    task_environment_.RunUntilIdle();
  }

 protected:
  scoped_refptr<CannedAppCacheHelper> CreateHelper() {
    return base::MakeRefCounted<CannedAppCacheHelper>(
        content::BrowserContext::GetDefaultStoragePartition(&browser_context_)
            ->GetAppCacheService());
  }

  static bool ContainsOrigin(
      const std::list<content::StorageUsageInfo>& collection,
      const url::Origin& origin) {
    return std::find_if(collection.begin(), collection.end(),
                        [&](const content::StorageUsageInfo& info) {
                          return info.origin == origin;
                        }) != collection.end();
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

TEST_F(CannedAppCacheHelperTest, SetInfo) {
  GURL manifest1("http://example1.com/manifest.xml");
  GURL manifest2("http://example2.com/path1/manifest.xml");
  GURL manifest3("http://example2.com/path2/manifest.xml");

  auto helper = CreateHelper();
  helper->Add(url::Origin::Create(manifest1));
  helper->Add(url::Origin::Create(manifest2));
  helper->Add(url::Origin::Create(manifest3));

  std::list<content::StorageUsageInfo> collection;
  base::RunLoop run_loop;
  helper->StartFetching(base::BindLambdaForTesting(
      [&](const std::list<content::StorageUsageInfo>& list) {
        collection = list;
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_EQ(2u, collection.size());
  EXPECT_TRUE(ContainsOrigin(collection, url::Origin::Create(manifest1)));
  EXPECT_TRUE(ContainsOrigin(collection, url::Origin::Create(manifest2)));
  for (const auto& info : collection) {
    EXPECT_EQ(0, info.total_size_bytes);
    EXPECT_EQ(base::Time(), info.last_modified);
  }
}

TEST_F(CannedAppCacheHelperTest, Unique) {
  GURL manifest("http://example.com/manifest.xml");

  auto helper = CreateHelper();
  helper->Add(url::Origin::Create(manifest));
  helper->Add(url::Origin::Create(manifest));

  std::list<content::StorageUsageInfo> collection;
  base::RunLoop run_loop;
  helper->StartFetching(base::BindLambdaForTesting(
      [&](const std::list<content::StorageUsageInfo>& list) {
        collection = list;
        run_loop.Quit();
      }));
  run_loop.Run();

  ASSERT_EQ(1u, collection.size());
  EXPECT_TRUE(ContainsOrigin(collection, url::Origin::Create(manifest)));
}

TEST_F(CannedAppCacheHelperTest, Empty) {
  GURL manifest("http://example.com/manifest.xml");

  auto helper = CreateHelper();

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(manifest));
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedAppCacheHelperTest, Delete) {
  GURL manifest1("http://example.com/manifest1.xml");
  GURL manifest2("http://foo.example.com/manifest2.xml");
  GURL manifest3("http://bar.example.com/manifest3.xml");

  auto helper = CreateHelper();

  EXPECT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(manifest1));
  helper->Add(url::Origin::Create(manifest2));
  helper->Add(url::Origin::Create(manifest3));
  EXPECT_FALSE(helper->empty());
  EXPECT_EQ(3u, helper->GetCount());
  helper->DeleteAppCaches(url::Origin::Create(manifest2));
  EXPECT_EQ(2u, helper->GetCount());
  EXPECT_FALSE(
      base::Contains(helper->GetOrigins(), url::Origin::Create(manifest2)));
}

TEST_F(CannedAppCacheHelperTest, IgnoreExtensionsAndDevTools) {
  GURL manifest1("chrome-extension://abcdefghijklmnopqrstuvwxyz/manifest.xml");
  GURL manifest2("devtools://abcdefghijklmnopqrstuvwxyz/manifest.xml");

  auto helper = CreateHelper();

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(manifest1));
  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(manifest2));
  ASSERT_TRUE(helper->empty());
}

}  // namespace browsing_data
