// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <list>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "components/browsing_data/content/browsing_data_helper_browsertest.h"
#include "components/browsing_data/content/database_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "storage/browser/database/database_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserContext;

namespace browsing_data {
namespace {
using TestCompletionCallback =
    BrowsingDataHelperCallback<content::StorageUsageInfo>;

const char kTestIdentifier1[] = "http_www.example.com_0";
const char kTestIdentifier2[] = "http_www.mysite.com_0";

class DatabaseHelperTest : public content::ContentBrowserTest {
 public:
  virtual void CreateDatabases() {
    storage::DatabaseTracker* db_tracker = shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition()
                                               ->GetDatabaseTracker();
    base::RunLoop run_loop;
    db_tracker->task_runner()->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          std::u16string db_name = u"db";
          std::u16string description = u"db_description";
          int64_t size;
          db_tracker->DatabaseOpened(kTestIdentifier1, db_name, description,
                                     &size);
          db_tracker->DatabaseClosed(kTestIdentifier1, db_name);
          base::FilePath db_path1 =
              db_tracker->GetFullDBFilePath(kTestIdentifier1, db_name);
          base::CreateDirectory(db_path1.DirName());
          ASSERT_EQ(0, base::WriteFile(db_path1, nullptr, 0));
          db_tracker->DatabaseOpened(kTestIdentifier2, db_name, description,
                                     &size);
          db_tracker->DatabaseClosed(kTestIdentifier2, db_name);
          base::FilePath db_path2 =
              db_tracker->GetFullDBFilePath(kTestIdentifier2, db_name);
          base::CreateDirectory(db_path2.DirName());
          ASSERT_EQ(0, base::WriteFile(db_path2, nullptr, 0));
          std::vector<storage::OriginInfo> origins;
          db_tracker->GetAllOriginsInfo(&origins);
          ASSERT_EQ(2U, origins.size());
        }),
        run_loop.QuitClosure());
    run_loop.Run();
  }
};

// Flaky, see https://crbug.com/1293136
IN_PROC_BROWSER_TEST_F(DatabaseHelperTest, DISABLED_FetchData) {
  CreateDatabases();
  auto database_helper = base::MakeRefCounted<DatabaseHelper>(
      shell()->web_contents()->GetBrowserContext());
  std::list<content::StorageUsageInfo> database_info_list;
  base::RunLoop run_loop;
  database_helper->StartFetching(base::BindLambdaForTesting(
      [&](const std::list<content::StorageUsageInfo>& list) {
        database_info_list = list;
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_EQ(2u, database_info_list.size());

  auto db_info_it = database_info_list.begin();
  EXPECT_EQ(url::Origin::Create(GURL("http://www.example.com")),
            db_info_it->storage_key.origin());
  EXPECT_EQ(url::Origin::Create(GURL("http://www.mysite.com")),
            std::next(db_info_it)->storage_key.origin());
}

IN_PROC_BROWSER_TEST_F(DatabaseHelperTest, CannedAddDatabase) {
  const url::Origin origin1 = url::Origin::Create(GURL("http://host1:1/"));
  const url::Origin origin2 = url::Origin::Create(GURL("http://host2:1/"));

  auto database_helper = base::MakeRefCounted<CannedDatabaseHelper>(
      shell()->web_contents()->GetBrowserContext());
  database_helper->Add(origin1);
  database_helper->Add(origin1);
  database_helper->Add(origin2);

  TestCompletionCallback callback;
  database_helper->StartFetching(base::BindOnce(
      &TestCompletionCallback::callback, base::Unretained(&callback)));

  std::list<content::StorageUsageInfo> result = callback.result();

  ASSERT_EQ(2u, result.size());
  auto info = result.begin();
  EXPECT_EQ(origin1, info->storage_key.origin());
  ++info;
  EXPECT_EQ(origin2, info->storage_key.origin());
}

IN_PROC_BROWSER_TEST_F(DatabaseHelperTest, CannedUnique) {
  const url::Origin origin = url::Origin::Create(GURL("http://host1:1/"));

  auto database_helper = base::MakeRefCounted<CannedDatabaseHelper>(
      shell()->web_contents()->GetBrowserContext());
  database_helper->Add(origin);
  database_helper->Add(origin);

  TestCompletionCallback callback;
  database_helper->StartFetching(base::BindOnce(
      &TestCompletionCallback::callback, base::Unretained(&callback)));

  std::list<content::StorageUsageInfo> result = callback.result();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(origin, result.begin()->storage_key.origin());
}
}  // namespace
}  // namespace browsing_data
