// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/add_page_task.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {

const char kTestNamespace[] = "default";
const int64_t kTestOfflineId1 = 1234LL;
const ClientId kTestClientId1(kTestNamespace, "1234");
const base::FilePath kTestFilePath(FILE_PATH_LITERAL("/test/path/file"));
const int64_t kTestFileSize = 876543LL;
const std::string kTestOrigin("abc.xyz");
const std::u16string kTestTitle = u"a title";
const int64_t kTestDownloadId = 767574LL;
const std::string kTestDigest("TesTIngDigEst==");
const std::string kTestAttribution = "attribution";
const std::string kTestSnippet = "snippet";

}  // namespace

class AddPageTaskTest : public ModelTaskTestBase {
 public:
  void ResetResults();
  void OnAddPageDone(AddPageResult result);
  AddPageTask::AddPageTaskCallback add_page_callback();

  void AddPage(const OfflinePageItem& page);
  bool CheckPageStored(const OfflinePageItem& page);

  const std::optional<AddPageResult>& last_add_page_result() {
    return last_add_page_result_;
  }

 private:
  std::optional<AddPageResult> last_add_page_result_;
  base::WeakPtrFactory<AddPageTaskTest> weak_ptr_factory_{this};
};

void AddPageTaskTest::ResetResults() {
  last_add_page_result_.reset();
}

void AddPageTaskTest::OnAddPageDone(AddPageResult result) {
  last_add_page_result_ = result;
}

AddPageTask::AddPageTaskCallback AddPageTaskTest::add_page_callback() {
  return base::BindOnce(&AddPageTaskTest::OnAddPageDone,
                        weak_ptr_factory_.GetWeakPtr());
}

void AddPageTaskTest::AddPage(const OfflinePageItem& page) {
  auto task = std::make_unique<AddPageTask>(store(), page, add_page_callback());
  RunTask(std::move(task));
}

bool AddPageTaskTest::CheckPageStored(const OfflinePageItem& page) {
  auto stored_page = store_test_util()->GetPageByOfflineId(page.offline_id);
  return stored_page && page == *stored_page;
}

TEST_F(AddPageTaskTest, AddPage) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page = generator()->CreateItem();
  AddPage(page);

  // Start checking if the page is added into the store.
  EXPECT_TRUE(CheckPageStored(page));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(AddPageResult::SUCCESS, last_add_page_result());
}

TEST_F(AddPageTaskTest, AddPageWithAllFieldsSet) {
  OfflinePageItem page(GURL("http://example.com"), kTestOfflineId1,
                       kTestClientId1, kTestFilePath, kTestFileSize,
                       base::Time::Now());
  page.request_origin = kTestOrigin;
  page.title = kTestTitle;
  page.original_url_if_different = GURL("http://other.page.com");
  page.system_download_id = kTestDownloadId;
  page.file_missing_time = base::Time::Now();
  page.digest = kTestDigest;
  page.attribution = kTestAttribution;
  page.snippet = kTestSnippet;

  AddPage(page);

  // Start checking if the page is added into the store.
  EXPECT_TRUE(CheckPageStored(page));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(AddPageResult::SUCCESS, last_add_page_result());
}

TEST_F(AddPageTaskTest, AddTwoPages) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItem();
  OfflinePageItem page2 = generator()->CreateItem();

  // Adding the first page.
  AddPage(page1);
  EXPECT_EQ(AddPageResult::SUCCESS, last_add_page_result());
  ResetResults();

  // Adding the second page.
  AddPage(page2);
  EXPECT_EQ(AddPageResult::SUCCESS, last_add_page_result());

  // Confirm two pages were added.
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(CheckPageStored(page1));
  EXPECT_TRUE(CheckPageStored(page2));
}

TEST_F(AddPageTaskTest, AddTwoIdenticalPages) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page = generator()->CreateItem();

  // Add the page for the first time.
  AddPage(page);
  EXPECT_TRUE(CheckPageStored(page));
  EXPECT_EQ(AddPageResult::SUCCESS, last_add_page_result());
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  ResetResults();

  // Add the page for the second time, the page should not be added since it
  // already exists in the store.
  AddPage(page);
  EXPECT_EQ(AddPageResult::ALREADY_EXISTS, last_add_page_result());
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
}

TEST_F(AddPageTaskTest, AddPageWithInvalidStore) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page = generator()->CreateItem();
  auto task = std::make_unique<AddPageTask>(nullptr, page, add_page_callback());
  RunTask(std::move(task));

  // Start checking if the page is added into the store.
  EXPECT_FALSE(CheckPageStored(page));
  EXPECT_EQ(AddPageResult::STORE_FAILURE, last_add_page_result());
  EXPECT_EQ(0LL, store_test_util()->GetPageCount());
}

}  // namespace offline_pages
