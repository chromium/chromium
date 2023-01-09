// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/shared_worker_helper.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace browsing_data {
namespace {

class CannedSharedWorkerHelperTest : public testing::Test {
 public:
  content::BrowserContext* browser_context() { return &browser_context_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

TEST_F(CannedSharedWorkerHelperTest, Empty) {
  const GURL worker("https://host1:1/worker.js");
  std::string name("test");
  const blink::StorageKey storage_key =
      blink::StorageKey(url::Origin::Create(worker));

  auto helper = base::MakeRefCounted<CannedSharedWorkerHelper>(
      browser_context()->GetDefaultStoragePartition());

  EXPECT_TRUE(helper->empty());
  helper->AddSharedWorker(worker, name, storage_key);
  EXPECT_FALSE(helper->empty());
  helper->Reset();
  EXPECT_TRUE(helper->empty());
}

TEST_F(CannedSharedWorkerHelperTest, Delete) {
  const GURL worker1("http://host1:9000/worker.js");
  std::string name1("name");
  const blink::StorageKey storage_key1(url::Origin::Create(worker1));
  const GURL worker2("https://example.com/worker.js");
  std::string name2("name");
  const blink::StorageKey storage_key2(url::Origin::Create(worker2));

  auto helper = base::MakeRefCounted<CannedSharedWorkerHelper>(
      browser_context()->GetDefaultStoragePartition());

  EXPECT_TRUE(helper->empty());
  helper->AddSharedWorker(worker1, name1, storage_key1);
  helper->AddSharedWorker(worker2, name2, storage_key2);
  EXPECT_EQ(2u, helper->GetSharedWorkerCount());
  helper->DeleteSharedWorker(worker2, name2, storage_key2);
  EXPECT_EQ(1u, helper->GetSharedWorkerCount());
}

TEST_F(CannedSharedWorkerHelperTest, IgnoreExtensionsAndDevTools) {
  const GURL worker1("chrome-extension://abcdefghijklmnopqrstuvwxyz/worker.js");
  const GURL worker2("devtools://abcdefghijklmnopqrstuvwxyz/worker.js");
  std::string name("name");
  const blink::StorageKey storage_key1(url::Origin::Create(worker1));
  const blink::StorageKey storage_key2(url::Origin::Create(worker2));

  auto helper = base::MakeRefCounted<CannedSharedWorkerHelper>(
      browser_context()->GetDefaultStoragePartition());

  EXPECT_TRUE(helper->empty());
  helper->AddSharedWorker(worker1, name, storage_key1);
  EXPECT_TRUE(helper->empty());
  helper->AddSharedWorker(worker2, name, storage_key2);
  EXPECT_TRUE(helper->empty());
}

}  // namespace
}  // namespace browsing_data
