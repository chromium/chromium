// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_store.h"

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

constexpr int64_t kTabId = 1;
constexpr char kUrl[] = "https://example.com/";

proto::PageContext TestContent(const std::string& title) {
  proto::PageContext page_context;
  page_context.mutable_annotated_page_content()
      ->mutable_main_frame_data()
      ->set_title(title);
  return page_context;
}

}  // namespace

class PageContentStoreTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = std::make_unique<PageContentStore>(db_path());
    auto os_crypt_async = os_crypt_async::GetTestOSCryptAsyncForTesting();
    base::RunLoop run_loop;
    os_crypt_async->GetInstance(base::BindOnce(
        [](PageContentStore* store, base::RunLoop* run_loop,
           os_crypt_async::Encryptor encryptor) {
          store->InitWithEncryptor(std::move(encryptor));
          run_loop->Quit();
        },
        store_.get(), &run_loop));
    run_loop.Run();
  }

  void TearDown() override { store_.reset(); }

  base::FilePath db_path() const {
    return temp_dir_.GetPath().AppendASCII("PageContentStoreTest.db");
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<PageContentStore> store_;
};

TEST_F(PageContentStoreTest, AddPageContent_TestMultipleTabs) {
  const GURL url1("https://example.com/1");
  const auto page_context1 = TestContent("test title 1");
  const base::Time visit_timestamp = base::Time::Now();
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_TRUE(store_->AddPageContent(url1, page_context1, visit_timestamp,
                                     extraction_timestamp, 1));

  const GURL url2("https://example.com/2");
  const auto page_context2 = TestContent("test title 2");
  EXPECT_TRUE(store_->AddPageContent(url2, page_context2, visit_timestamp,
                                     extraction_timestamp, 2));

  std::optional<proto::PageContext> got_page_context1 =
      store_->GetPageContent(url1);
  ASSERT_TRUE(got_page_context1.has_value());
  EXPECT_EQ(
      page_context1.annotated_page_content().main_frame_data().title(),
      got_page_context1->annotated_page_content().main_frame_data().title());

  std::optional<proto::PageContext> got_page_context2 =
      store_->GetPageContent(url2);
  ASSERT_TRUE(got_page_context2.has_value());
  EXPECT_EQ(
      page_context2.annotated_page_content().main_frame_data().title(),
      got_page_context2->annotated_page_content().main_frame_data().title());
}

TEST_F(PageContentStoreTest, AddPageContent_SucceedsOnDuplicate) {
  const GURL url(kUrl);
  const auto page_context1 = TestContent("test title 1");
  const base::Time visit_timestamp = base::Time::Now();
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_TRUE(store_->AddPageContent(url, page_context1, visit_timestamp,
                                     extraction_timestamp, kTabId));

  const auto page_context2 = TestContent("test title 2");
  EXPECT_TRUE(store_->AddPageContent(url, page_context2, visit_timestamp,
                                     extraction_timestamp, kTabId));
  std::optional<proto::PageContext> got_page_context =
      store_->GetPageContentForTab(kTabId);
  ASSERT_TRUE(got_page_context.has_value());
  EXPECT_EQ(
      page_context2.annotated_page_content().main_frame_data().title(),
      got_page_context->annotated_page_content().main_frame_data().title());
}

TEST_F(PageContentStoreTest, AddPageContent_SucceedsAfterDelete) {
  const GURL url(kUrl);
  const auto apc1 = TestContent("test title 1");
  const base::Time visit_timestamp = base::Time::Now();
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_TRUE(store_->AddPageContent(url, TestContent("test title 1"),
                                     visit_timestamp, extraction_timestamp,
                                     kTabId));

  EXPECT_TRUE(store_->DeletePageContentForTab(kTabId));

  const auto page_context2 = TestContent("test title 2");
  EXPECT_TRUE(store_->AddPageContent(url, page_context2, visit_timestamp,
                                     extraction_timestamp, kTabId));

  std::optional<proto::PageContext> got_page_context =
      store_->GetPageContent(url);
  ASSERT_TRUE(got_page_context.has_value());
  EXPECT_EQ(
      page_context2.annotated_page_content().main_frame_data().title(),
      got_page_context->annotated_page_content().main_frame_data().title());
}

TEST_F(PageContentStoreTest, GetPageContent_ReturnsMostRecent) {
  const GURL url(kUrl);
  const auto page_context1 = TestContent("old title");
  const base::Time visit_timestamp1 = base::Time::Now() - base::Days(1);
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_TRUE(store_->AddPageContent(url, page_context1, visit_timestamp1,
                                     extraction_timestamp, 1));

  const auto apc2 = TestContent("new title");
  const base::Time visit_timestamp2 = base::Time::Now();
  EXPECT_TRUE(store_->AddPageContent(url, apc2, visit_timestamp2,
                                     extraction_timestamp, 2));

  std::optional<proto::PageContext> got_apc = store_->GetPageContent(url);
  ASSERT_TRUE(got_apc.has_value());
  EXPECT_EQ(apc2.annotated_page_content().main_frame_data().title(),
            got_apc->annotated_page_content().main_frame_data().title());
}

TEST_F(PageContentStoreTest, AddPageContent_NullTabId) {
  const GURL url1("https://example.com/1");
  const auto page_context1 = TestContent("test title 1");
  const base::Time visit_timestamp = base::Time::Now();
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_TRUE(store_->AddPageContent(url1, page_context1, visit_timestamp,
                                     extraction_timestamp, std::nullopt));

  const GURL url2("https://example.com/2");
  const auto page_context2 = TestContent("test title 2");
  EXPECT_TRUE(store_->AddPageContent(url2, page_context2, visit_timestamp,
                                     extraction_timestamp, std::nullopt));

  std::optional<proto::PageContext> got_page_context1 =
      store_->GetPageContent(url1);
  ASSERT_TRUE(got_page_context1.has_value());
  EXPECT_EQ(
      page_context1.annotated_page_content().main_frame_data().title(),
      got_page_context1->annotated_page_content().main_frame_data().title());

  std::optional<proto::PageContext> got_page_context2 =
      store_->GetPageContent(url2);
  ASSERT_TRUE(got_page_context2.has_value());
  EXPECT_EQ(
      page_context2.annotated_page_content().main_frame_data().title(),
      got_page_context2->annotated_page_content().main_frame_data().title());
}

TEST_F(PageContentStoreTest, DeletePageContentOlderThan) {
  const GURL url(kUrl);
  const auto apc = TestContent("test title");
  const base::Time visit_timestamp = base::Time::Now() - base::Days(8);
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_TRUE(store_->AddPageContent(url, TestContent("test title"),
                                     visit_timestamp, extraction_timestamp,
                                     kTabId));

  EXPECT_TRUE(
      store_->DeletePageContentOlderThan(base::Time::Now() - base::Days(7)));

  std::optional<proto::PageContext> got_apc = store_->GetPageContent(url);
  ASSERT_FALSE(got_apc.has_value());
}

TEST_F(PageContentStoreTest, DeletePageContentOlderThan_RespectsMaxLimit) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      page_content_annotations::features::kPageContentCache,
      {{page_content_annotations::features::kPageContentCacheMaxTabs.name,
        "2"}});

  const base::Time now = base::Time::Now();
  const base::Time extraction_timestamp = now;

  // Add 3 entries.
  EXPECT_TRUE(store_->AddPageContent(
      GURL("https://example.com/1"), TestContent("title 1"),
      now - base::Days(3), extraction_timestamp, 1));
  EXPECT_TRUE(store_->AddPageContent(
      GURL("https://example.com/2"), TestContent("title 2"),
      now - base::Days(2), extraction_timestamp, 2));
  EXPECT_TRUE(store_->AddPageContent(
      GURL("https://example.com/3"), TestContent("title 3"),
      now - base::Days(1), extraction_timestamp, 3));

  // Everything is newer than 4 days ago, but we should only keep 2.
  EXPECT_TRUE(store_->DeletePageContentOlderThan(now - base::Days(4)));

  // The oldest one should be gone.
  std::optional<proto::PageContext> got_apc = store_->GetPageContentForTab(1);
  ASSERT_FALSE(got_apc.has_value());

  // The two newest should still be there.
  got_apc = store_->GetPageContentForTab(2);
  ASSERT_TRUE(got_apc.has_value());
  got_apc = store_->GetPageContentForTab(3);
  ASSERT_TRUE(got_apc.has_value());
}

TEST_F(PageContentStoreTest, DeletePageContentForTab) {
  const GURL url(kUrl);
  const auto apc = TestContent("test title");
  const base::Time visit_timestamp = base::Time::Now();
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_TRUE(store_->AddPageContent(url, TestContent("test title"),
                                     visit_timestamp, extraction_timestamp,
                                     kTabId));

  EXPECT_TRUE(store_->DeletePageContentForTab(kTabId));

  std::optional<proto::PageContext> got_apc = store_->GetPageContent(url);
  ASSERT_FALSE(got_apc.has_value());
}

TEST_F(PageContentStoreTest, DeletePageContentForTabs) {
  const base::Time visit_timestamp = base::Time::Now();
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_TRUE(store_->AddPageContent(GURL("https://example.com/1"),
                                     TestContent("test title 1"),
                                     visit_timestamp, extraction_timestamp, 1));
  EXPECT_TRUE(store_->AddPageContent(GURL("https://example.com/2"),
                                     TestContent("test title 2"),
                                     visit_timestamp, extraction_timestamp, 2));
  EXPECT_TRUE(store_->AddPageContent(GURL("https://example.com/3"),
                                     TestContent("test title 3"),
                                     visit_timestamp, extraction_timestamp, 3));

  EXPECT_TRUE(store_->DeletePageContentForTabs({1, 3}));

  EXPECT_FALSE(store_->GetPageContentForTab(1).has_value());
  EXPECT_TRUE(store_->GetPageContentForTab(2).has_value());
  EXPECT_FALSE(store_->GetPageContentForTab(3).has_value());

  // Deleting a non-existent tab ID should not fail.
  EXPECT_TRUE(store_->DeletePageContentForTabs({4}));
  EXPECT_TRUE(store_->GetPageContentForTab(2).has_value());

  // Deleting with an empty set should not fail.
  EXPECT_TRUE(store_->DeletePageContentForTabs({}));
  EXPECT_TRUE(store_->GetPageContentForTab(2).has_value());
}

TEST_F(PageContentStoreTest, GetPageContentForTab) {
  const GURL url(kUrl);
  const auto apc = TestContent("test title");
  const base::Time visit_timestamp = base::Time::Now();
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_TRUE(store_->AddPageContent(url, TestContent("test title"),
                                     visit_timestamp, extraction_timestamp,
                                     kTabId));

  std::optional<proto::PageContext> got_apc =
      store_->GetPageContentForTab(kTabId);
  ASSERT_TRUE(got_apc.has_value());
  EXPECT_EQ(apc.annotated_page_content().main_frame_data().title(),
            got_apc->annotated_page_content().main_frame_data().title());
}

TEST_F(PageContentStoreTest, DeleteAllEntries) {
  const GURL url(kUrl);
  const auto apc = TestContent("test title");
  const base::Time visit_timestamp = base::Time::Now();
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_TRUE(store_->AddPageContent(url, TestContent("test title"),
                                     visit_timestamp, extraction_timestamp,
                                     kTabId));

  EXPECT_TRUE(store_->DeleteAllEntries());

  std::optional<proto::PageContext> got_apc = store_->GetPageContent(url);
  ASSERT_FALSE(got_apc.has_value());
}

TEST_F(PageContentStoreTest, GetAllTabIds) {
  const base::Time visit_timestamp = base::Time::Now();
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_TRUE(store_->AddPageContent(GURL("https://example.com/1"),
                                     TestContent("test title 1"),
                                     visit_timestamp, extraction_timestamp, 1));
  EXPECT_TRUE(store_->AddPageContent(GURL("https://example.com/2"),
                                     TestContent("test title 2"),
                                     visit_timestamp, extraction_timestamp, 2));
  EXPECT_TRUE(store_->AddPageContent(
      GURL("https://example.com/3"), TestContent("test title 3"),
      visit_timestamp, extraction_timestamp, std::nullopt));

  std::vector<int64_t> tab_ids = store_->GetAllTabIds();
  EXPECT_EQ(tab_ids.size(), 2u);
  EXPECT_EQ(tab_ids[0], 1);
  EXPECT_EQ(tab_ids[1], 2);
}

class PageContentStoreNoEncryptorTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = std::make_unique<PageContentStore>(db_path());
  }

  void TearDown() override { store_.reset(); }

  base::FilePath db_path() const {
    return temp_dir_.GetPath().AppendASCII("PageContentStoreTest.db");
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<PageContentStore> store_;
};

TEST_F(PageContentStoreNoEncryptorTest, AddPageContentFails) {
  const GURL url(kUrl);
  const auto page_context = TestContent("test title");
  const base::Time visit_timestamp = base::Time::Now();
  const base::Time extraction_timestamp = base::Time::Now();

  EXPECT_FALSE(store_->AddPageContent(url, page_context, visit_timestamp,
                                      extraction_timestamp, kTabId));
}

TEST_F(PageContentStoreNoEncryptorTest, GetPageContentFails) {
  const GURL url(kUrl);
  std::optional<proto::PageContext> got_apc = store_->GetPageContent(url);
  ASSERT_FALSE(got_apc.has_value());
}

TEST_F(PageContentStoreNoEncryptorTest, GetPageContentForNonExistentTabId) {
  std::optional<proto::PageContext> got_apc =
      store_->GetPageContentForTab(kTabId);
  ASSERT_FALSE(got_apc.has_value());
}

TEST_F(PageContentStoreNoEncryptorTest,
       DeletePageContentOlderThanWithNoMatchingEntries) {
  EXPECT_TRUE(
      store_->DeletePageContentOlderThan(base::Time::Now() - base::Days(7)));
}

TEST_F(PageContentStoreNoEncryptorTest, DeletePageContentForNonExistentTab) {
  EXPECT_TRUE(store_->DeletePageContentForTab(kTabId));
}

}  // namespace optimization_guide
