// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_content_store.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

namespace {

ArticleEntry CreateEntry(std::string entry_id,
                         GURL page_url1 = GURL(),
                         GURL page_url2 = GURL(),
                         GURL page_url3 = GURL()) {
  ArticleEntry entry;
  entry.entry_id = entry_id;
  if (!page_url1.is_empty()) {
    entry.pages.push_back(page_url1);
  }
  if (!page_url2.is_empty()) {
    entry.pages.push_back(page_url2);
  }
  if (!page_url3.is_empty()) {
    entry.pages.push_back(page_url3);
  }
  return entry;
}

DistilledArticleProto CreateDistilledArticleForEntry(
    const ArticleEntry& entry) {
  DistilledArticleProto article;
  for (const GURL& url : entry.pages) {
    DistilledPageProto* page = article.add_pages();
    page->set_url(url.spec());
    page->set_html("<div>" + url.spec() + "</div>");
  }
  return article;
}

}  // namespace

class InMemoryContentStoreTest : public testing::Test {
 public:
  void OnLoadCallback(bool success,
                      std::unique_ptr<DistilledArticleProto> proto) {
    load_success_ = success;
    loaded_proto_ = std::move(proto);
  }

  void OnSaveCallback(bool success) { save_success_ = success; }

 protected:
  // testing::Test implementation:
  void SetUp() override {
    store_ =
        std::make_unique<InMemoryContentStore>(kDefaultMaxNumCachedEntries);
    save_success_ = false;
    load_success_ = false;
    loaded_proto_.reset();
  }

  std::unique_ptr<InMemoryContentStore> store_;
  bool save_success_;
  bool load_success_;
  std::unique_ptr<DistilledArticleProto> loaded_proto_;
};

// Tests whether saving and then loading a single article works as expected.
TEST_F(InMemoryContentStoreTest, SaveAndLoadSingleArticle) {
  base::test::SingleThreadTaskEnvironment task_environment;
  const ArticleEntry entry =
      CreateEntry("test-id", GURL("https://url1"), GURL("https://url2"),
                  GURL("https://url3"));
  const DistilledArticleProto stored_proto =
      CreateDistilledArticleForEntry(entry);
  store_->SaveContent(entry, stored_proto,
                      base::BindOnce(&InMemoryContentStoreTest::OnSaveCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(save_success_);
  save_success_ = false;

  store_->LoadContent(entry,
                      base::BindOnce(&InMemoryContentStoreTest::OnLoadCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(load_success_);
  EXPECT_EQ(stored_proto.SerializeAsString(),
            loaded_proto_->SerializeAsString());
}

// Tests that loading articles which have never been stored, yields a callback
// where success is false.
TEST_F(InMemoryContentStoreTest, LoadNonExistentArticle) {
  base::test::SingleThreadTaskEnvironment task_environment;
  const ArticleEntry entry =
      CreateEntry("bogus-id", GURL("https://url1"), GURL("https://url2"),
                  GURL("https://url3"));
  store_->LoadContent(entry,
                      base::BindOnce(&InMemoryContentStoreTest::OnLoadCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(load_success_);
}

// Verifies that content store can store multiple articles, and that ordering
// of save and store does not matter when the total number of articles does not
// exceed |kDefaultMaxNumCachedEntries|.
TEST_F(InMemoryContentStoreTest, SaveAndLoadMultipleArticles) {
  base::test::SingleThreadTaskEnvironment task_environment;
  // Store first article.
  const ArticleEntry first_entry =
      CreateEntry("first", GURL("https://url1"), GURL("https://url2"),
                  GURL("https://url3"));
  const DistilledArticleProto first_stored_proto =
      CreateDistilledArticleForEntry(first_entry);
  store_->SaveContent(first_entry, first_stored_proto,
                      base::BindOnce(&InMemoryContentStoreTest::OnSaveCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(save_success_);
  save_success_ = false;

  // Store second article.
  const ArticleEntry second_entry =
      CreateEntry("second", GURL("https://url4"), GURL("https://url5"),
                  GURL("https://url6"));
  const DistilledArticleProto second_stored_proto =
      CreateDistilledArticleForEntry(second_entry);
  store_->SaveContent(second_entry, second_stored_proto,
                      base::BindOnce(&InMemoryContentStoreTest::OnSaveCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(save_success_);
  save_success_ = false;

  // Load second article.
  store_->LoadContent(second_entry,
                      base::BindOnce(&InMemoryContentStoreTest::OnLoadCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(load_success_);
  load_success_ = false;
  EXPECT_EQ(second_stored_proto.SerializeAsString(),
            loaded_proto_->SerializeAsString());
  loaded_proto_.reset();

  // Load first article.
  store_->LoadContent(first_entry,
                      base::BindOnce(&InMemoryContentStoreTest::OnLoadCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(load_success_);
  EXPECT_EQ(first_stored_proto.SerializeAsString(),
            loaded_proto_->SerializeAsString());
}

// Verifies that the content store does not store unlimited number of articles,
// but expires the oldest ones when the limit for number of articles is reached.
TEST_F(InMemoryContentStoreTest, SaveAndLoadMoreThanMaxArticles) {
  base::test::SingleThreadTaskEnvironment task_environment;

  // Create a new store with only |kMaxNumArticles| articles as the limit.
  const int kMaxNumArticles = 3;
  store_ = std::make_unique<InMemoryContentStore>(kMaxNumArticles);

  // Store first article.
  const ArticleEntry first_entry =
      CreateEntry("first", GURL("https://url1"), GURL("https://url2"),
                  GURL("https://url3"));
  const DistilledArticleProto first_stored_proto =
      CreateDistilledArticleForEntry(first_entry);
  store_->SaveContent(first_entry, first_stored_proto,
                      base::BindOnce(&InMemoryContentStoreTest::OnSaveCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(save_success_);
  save_success_ = false;

  // Store second article.
  const ArticleEntry second_entry =
      CreateEntry("second", GURL("https://url4"), GURL("https://url5"),
                  GURL("https://url6"));
  const DistilledArticleProto second_stored_proto =
      CreateDistilledArticleForEntry(second_entry);
  store_->SaveContent(second_entry, second_stored_proto,
                      base::BindOnce(&InMemoryContentStoreTest::OnSaveCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(save_success_);
  save_success_ = false;

  // Store third article.
  const ArticleEntry third_entry =
      CreateEntry("third", GURL("https://url7"), GURL("https://url8"),
                  GURL("https://url9"));
  const DistilledArticleProto third_stored_proto =
      CreateDistilledArticleForEntry(third_entry);
  store_->SaveContent(third_entry, third_stored_proto,
                      base::BindOnce(&InMemoryContentStoreTest::OnSaveCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(save_success_);
  save_success_ = false;

  // Load first article. This will make the first article the most recent
  // accessed article.
  store_->LoadContent(first_entry,
                      base::BindOnce(&InMemoryContentStoreTest::OnLoadCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(load_success_);
  load_success_ = false;
  EXPECT_EQ(first_stored_proto.SerializeAsString(),
            loaded_proto_->SerializeAsString());
  loaded_proto_.reset();

  // Store fourth article.
  const ArticleEntry fourth_entry =
      CreateEntry("fourth", GURL("https://url10"), GURL("https://url11"),
                  GURL("https://url12"));
  const DistilledArticleProto fourth_stored_proto =
      CreateDistilledArticleForEntry(fourth_entry);
  store_->SaveContent(fourth_entry, fourth_stored_proto,
                      base::BindOnce(&InMemoryContentStoreTest::OnSaveCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(save_success_);
  save_success_ = false;

  // Load second article, which by now is the oldest accessed article, since
  // the first article has been loaded once.
  store_->LoadContent(second_entry,
                      base::BindOnce(&InMemoryContentStoreTest::OnLoadCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  // Since the store can only contain |kMaxNumArticles| entries, this load
  // should fail.
  EXPECT_FALSE(load_success_);
}

// Tests whether saving and then loading a single article works as expected.
TEST_F(InMemoryContentStoreTest, LookupArticleByURL) {
  base::test::SingleThreadTaskEnvironment task_environment;
  const ArticleEntry entry =
      CreateEntry("test-id", GURL("https://url1"), GURL("https://url2"),
                  GURL("https://url3"));
  const DistilledArticleProto stored_proto =
      CreateDistilledArticleForEntry(entry);
  store_->SaveContent(entry, stored_proto,
                      base::BindOnce(&InMemoryContentStoreTest::OnSaveCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(save_success_);
  save_success_ = false;

  // Create an entry where the entry ID does not match, but the first URL does.
  const ArticleEntry lookup_entry1 =
      CreateEntry("lookup-id", GURL("https://url1"));
  store_->LoadContent(lookup_entry1,
                      base::BindOnce(&InMemoryContentStoreTest::OnLoadCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(load_success_);
  EXPECT_EQ(stored_proto.SerializeAsString(),
            loaded_proto_->SerializeAsString());

  // Create an entry where the entry ID does not match, but the second URL does.
  const ArticleEntry lookup_entry2 =
      CreateEntry("lookup-id", GURL("bogus"), GURL("https://url2"));
  store_->LoadContent(lookup_entry2,
                      base::BindOnce(&InMemoryContentStoreTest::OnLoadCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(load_success_);
  EXPECT_EQ(stored_proto.SerializeAsString(),
            loaded_proto_->SerializeAsString());
}

// Verifies that the content store does not store unlimited number of articles,
// but expires the oldest ones when the limit for number of articles is reached.
TEST_F(InMemoryContentStoreTest, LoadArticleByURLAfterExpungedFromCache) {
  base::test::SingleThreadTaskEnvironment task_environment;

  // Create a new store with only |kMaxNumArticles| articles as the limit.
  const int kMaxNumArticles = 1;
  store_ = std::make_unique<InMemoryContentStore>(kMaxNumArticles);

  // Store an article.
  const ArticleEntry first_entry =
      CreateEntry("first", GURL("https://url1"), GURL("https://url2"),
                  GURL("https://url3"));
  const DistilledArticleProto first_stored_proto =
      CreateDistilledArticleForEntry(first_entry);
  store_->SaveContent(first_entry, first_stored_proto,
                      base::BindOnce(&InMemoryContentStoreTest::OnSaveCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(save_success_);
  save_success_ = false;

  // Looking up the first entry by URL should succeed when it is still in the
  // cache.
  const ArticleEntry first_entry_lookup =
      CreateEntry("lookup-id", GURL("https://url1"));
  store_->LoadContent(first_entry_lookup,
                      base::BindOnce(&InMemoryContentStoreTest::OnLoadCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(load_success_);
  EXPECT_EQ(first_stored_proto.SerializeAsString(),
            loaded_proto_->SerializeAsString());

  // Store second article. This will remove the first article from the cache.
  const ArticleEntry second_entry =
      CreateEntry("second", GURL("https://url4"), GURL("https://url5"),
                  GURL("https://url6"));
  const DistilledArticleProto second_stored_proto =
      CreateDistilledArticleForEntry(second_entry);
  store_->SaveContent(second_entry, second_stored_proto,
                      base::BindOnce(&InMemoryContentStoreTest::OnSaveCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(save_success_);
  save_success_ = false;

  // Looking up the first entry by URL should fail when it is not in the cache.
  store_->LoadContent(first_entry_lookup,
                      base::BindOnce(&InMemoryContentStoreTest::OnLoadCallback,
                                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(load_success_);
}

}  // namespace dom_distiller
