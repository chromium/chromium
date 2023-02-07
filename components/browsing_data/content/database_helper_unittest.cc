// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/database_helper.h"

#include "base/memory/scoped_refptr.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "storage/common/database/database_identifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {
namespace {

using storage::DatabaseIdentifier;

class CannedDatabaseHelperTest : public testing::Test {
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(CannedDatabaseHelperTest, Empty) {
  content::TestBrowserContext browser_context;

  const GURL origin("http://host1:1/");

  auto helper = base::MakeRefCounted<CannedDatabaseHelper>(
      browser_context.GetDefaultStoragePartition());

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin));
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedDatabaseHelperTest, Delete) {
  content::TestBrowserContext browser_context;

  const GURL origin1("http://host1:9000");
  const GURL origin2("http://example.com");
  const GURL origin3("http://foo.example.com");

  auto helper = base::MakeRefCounted<CannedDatabaseHelper>(
      browser_context.GetDefaultStoragePartition());

  EXPECT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin1));
  helper->Add(url::Origin::Create(origin2));
  helper->Add(url::Origin::Create(origin3));
  EXPECT_EQ(3u, helper->GetCount());
  helper->DeleteDatabase(url::Origin::Create(origin2));
  EXPECT_EQ(2u, helper->GetCount());
  helper->DeleteDatabase(url::Origin::Create(origin2));
  EXPECT_EQ(2u, helper->GetCount());
}

TEST_F(CannedDatabaseHelperTest, IgnoreExtensionsAndDevTools) {
  content::TestBrowserContext browser_context;

  const GURL origin1("chrome-extension://abcdefghijklmnopqrstuvwxyz/");
  const GURL origin2("devtools://abcdefghijklmnopqrstuvwxyz/");

  auto helper = base::MakeRefCounted<CannedDatabaseHelper>(
      browser_context.GetDefaultStoragePartition());

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin1));
  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin2));
  ASSERT_TRUE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

}  // namespace
}  // namespace browsing_data
