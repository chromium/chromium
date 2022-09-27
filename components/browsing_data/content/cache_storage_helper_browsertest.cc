// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "components/browsing_data/content/browsing_data_helper_browsertest.h"
#include "components/browsing_data/content/cache_storage_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {
namespace {
using TestCompletionCallback =
    BrowsingDataHelperCallback<content::StorageUsageInfo>;

class CacheStorageHelperTest : public content::ContentBrowserTest {
 public:
  content::StoragePartition* storage_partition() const {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition();
  }

  scoped_refptr<CannedCacheStorageHelper> MakeHelper() {
    return base::MakeRefCounted<CannedCacheStorageHelper>(storage_partition());
  }
};

IN_PROC_BROWSER_TEST_F(CacheStorageHelperTest, CannedAddCacheStorage) {
  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");

  auto helper = MakeHelper();
  helper->Add(url::Origin::Create(origin1));
  helper->Add(url::Origin::Create(origin2));

  TestCompletionCallback callback;
  helper->StartFetching(base::BindOnce(&TestCompletionCallback::callback,
                                       base::Unretained(&callback)));

  std::list<content::StorageUsageInfo> result = callback.result();

  ASSERT_EQ(2U, result.size());
  auto info = result.begin();
  EXPECT_EQ(origin1, info->storage_key.origin().GetURL());
  info++;
  EXPECT_EQ(origin2, info->storage_key.origin().GetURL());
}

IN_PROC_BROWSER_TEST_F(CacheStorageHelperTest, CannedUnique) {
  const GURL origin("http://host1:1/");

  auto helper = MakeHelper();
  helper->Add(url::Origin::Create(origin));
  helper->Add(url::Origin::Create(origin));

  TestCompletionCallback callback;
  helper->StartFetching(base::BindOnce(&TestCompletionCallback::callback,
                                       base::Unretained(&callback)));

  std::list<content::StorageUsageInfo> result = callback.result();

  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(origin, result.begin()->storage_key.origin().GetURL());
}
}  // namespace
}  // namespace browsing_data
