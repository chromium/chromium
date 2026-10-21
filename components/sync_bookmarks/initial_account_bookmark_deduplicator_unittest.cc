// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/initial_account_bookmark_deduplicator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/test/mock_bookmark_model_observer.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync_bookmarks/test_node_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace sync_bookmarks {

namespace {

using bookmarks::BookmarkModel;
using bookmarks::BookmarkModelObserver;
using bookmarks::MockBookmarkModelObserver;
using sync_bookmarks::test::FolderBuilder;
using sync_bookmarks::test::UrlBuilder;
using testing::_;
using testing::ElementsAre;
using testing::InSequence;
using testing::IsEmpty;
using testing::NiceMock;
using testing::SizeIs;

class InitialAccountBookmarkDeduplicatorTest : public testing::Test {
 public:
  InitialAccountBookmarkDeduplicatorTest() {
    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModel();
    // Enable account storage, which is where the deduplicator will look for
    // the canonical version of the data.
    bookmark_model_->CreateAccountPermanentFolders();
    bookmark_deduplicator_ =
        std::make_unique<InitialAccountBookmarkDeduplicator>(
            bookmark_model_.get());
  }

 protected:
  void AddLocalNodes(
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_bookmark_bar) {
    FolderBuilder::AddChildrenTo(bookmark_model_.get(),
                                 bookmark_model_->bookmark_bar_node(),
                                 children_of_bookmark_bar);
  }

  void AddAccountNodes(
      const std::vector<FolderBuilder::FolderOrUrl>& children_of_bookmark_bar) {
    FolderBuilder::AddChildrenTo(bookmark_model_.get(),
                                 bookmark_model_->account_bookmark_bar_node(),
                                 children_of_bookmark_bar);
  }

  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<InitialAccountBookmarkDeduplicator> bookmark_deduplicator_;
};

TEST_F(InitialAccountBookmarkDeduplicatorTest, ShouldDoNothingIfNoDuplicates) {
  AddLocalNodes({UrlBuilder(u"Local", GURL("http://local.com"))});
  AddAccountNodes({UrlBuilder(u"Account", GURL("http://account.com"))});

  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  ASSERT_THAT(local_bar->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children(), SizeIs(1));

  bookmark_deduplicator_->Deduplicate();

  // No changes should have been made.
  EXPECT_THAT(local_bar->children(), SizeIs(1));
  EXPECT_THAT(account_bar->children(), SizeIs(1));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest, ShouldRemoveDuplicateUrl) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const GURL url("http://example.com");
  const std::u16string title = u"Title";

  AddLocalNodes({UrlBuilder(title, url).SetUuid(uuid)});
  AddAccountNodes({UrlBuilder(title, url).SetUuid(uuid)});

  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  ASSERT_THAT(local_bar->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children(), SizeIs(1));

  bookmark_deduplicator_->Deduplicate();

  // The local bookmark should have been removed.
  EXPECT_THAT(local_bar->children(), IsEmpty());
  EXPECT_THAT(account_bar->children(), SizeIs(1));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest, ShouldRemoveDuplicateFolder) {
  const base::Uuid folder_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid child_uuid = base::Uuid::GenerateRandomV4();
  const GURL child_url("http://child.com");
  const std::u16string folder_title = u"Folder";
  const std::u16string child_title = u"Child";

  AddLocalNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren(
               {UrlBuilder(child_title, child_url).SetUuid(child_uuid)})});
  AddAccountNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren(
               {UrlBuilder(child_title, child_url).SetUuid(child_uuid)})});

  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  ASSERT_THAT(local_bar->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children(), SizeIs(1));
  ASSERT_THAT(local_bar->children()[0]->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children()[0]->children(), SizeIs(1));

  bookmark_deduplicator_->Deduplicate();

  // The local folder and its contents should have been removed.
  EXPECT_THAT(local_bar->children(), IsEmpty());
  EXPECT_THAT(account_bar->children(), SizeIs(1));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldRemoveLocalFolderIfItIsASubgraphOfAccountFolder) {
  const base::Uuid folder_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid child_uuid = base::Uuid::GenerateRandomV4();
  const GURL child_url("http://child.com");
  const std::u16string folder_title = u"Folder";
  const std::u16string child_title = u"Child";

  AddLocalNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren(
               {UrlBuilder(child_title, child_url).SetUuid(child_uuid)})});
  AddAccountNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren({UrlBuilder(child_title, child_url).SetUuid(child_uuid),
                         UrlBuilder(u"Extra", GURL("http://extra.com"))})});

  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  ASSERT_THAT(local_bar->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children(), SizeIs(1));
  ASSERT_THAT(local_bar->children()[0]->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children()[0]->children(), SizeIs(2));

  bookmark_deduplicator_->Deduplicate();

  // The local folder should have been removed because it's a duplicate.
  EXPECT_THAT(local_bar->children(), IsEmpty());
  EXPECT_THAT(account_bar->children(), SizeIs(1));
  EXPECT_THAT(account_bar->children()[0]->children(), SizeIs(2));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldNotRemoveLocalFolderIfChildUrlDiffers) {
  const base::Uuid folder_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid child_uuid = base::Uuid::GenerateRandomV4();
  const std::u16string folder_title = u"Folder";
  const std::u16string child_title = u"Child";

  AddLocalNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren({UrlBuilder(child_title, GURL("http://local-child.com"))
                             .SetUuid(child_uuid)})});
  AddAccountNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren(
               {UrlBuilder(child_title, GURL("http://account-child.com"))
                    .SetUuid(child_uuid)})});

  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  ASSERT_THAT(local_bar->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children(), SizeIs(1));

  bookmark_deduplicator_->Deduplicate();

  // The local folder should NOT have been removed.
  EXPECT_THAT(local_bar->children(), SizeIs(1));
  EXPECT_THAT(account_bar->children(), SizeIs(1));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldHandleVariousDuplicationScenarios) {
  const base::Uuid social_folder_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid twitter_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid linkedin_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid shopping_folder_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid amazon_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid ebay_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid news_folder_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid nyt_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid bbc_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid email_uuid = base::Uuid::GenerateRandomV4();

  // 1. Account Bookmarks
  // 🗂️ Bookmarks Bar (Account)
  // ├── 📁 Social
  // │   ├── 🔖 Twitter
  // │   └── 🔖 LinkedIn
  // ├── 📁 Shopping
  // │   ├── 🔖 Amazon
  // │   └── 🔖 eBay
  // ├── 📁 News
  // │   ├── 🔖 NYT
  // │   └── 🔖 BBC
  // └── 🔖 Email
  AddAccountNodes(
      {FolderBuilder(u"Social")
           .SetUuid(social_folder_uuid)
           .SetChildren({UrlBuilder(u"Twitter", GURL("http://twitter.com"))
                             .SetUuid(twitter_uuid),
                         UrlBuilder(u"LinkedIn", GURL("http://linkedin.com"))
                             .SetUuid(linkedin_uuid)}),
       FolderBuilder(u"Shopping")
           .SetUuid(shopping_folder_uuid)
           .SetChildren({UrlBuilder(u"Amazon", GURL("http://amazon.com"))
                             .SetUuid(amazon_uuid),
                         UrlBuilder(u"eBay", GURL("http://ebay.com"))
                             .SetUuid(ebay_uuid)}),
       FolderBuilder(u"News")
           .SetUuid(news_folder_uuid)
           .SetChildren(
               {UrlBuilder(u"NYT", GURL("http://nyt.com")).SetUuid(nyt_uuid),
                UrlBuilder(u"BBC", GURL("http://bbc.com")).SetUuid(bbc_uuid)}),
       UrlBuilder(u"Email", GURL("http://email.com")).SetUuid(email_uuid)});

  // 2. Local Bookmarks (BEFORE Deduplication)
  // 🗂️ Bookmarks Bar (Local)
  // ├── 📁 Social
  // │   ├── 🔖 LinkedIn
  // │   └── 🔖 Twitter
  // ├── 📁 Shopping
  // │   ├── 🔖 Amazon
  // │   ├── 🔖 eBay
  // │   └── 🔖 Etsy
  // ├── 📁 News
  // │   └── 🔖 NYT
  // ├── 📁 Finance
  // │   └── 🔖 Bank
  // ├── 🔖 Email
  // └── 🔖 Maps
  AddLocalNodes(
      {FolderBuilder(u"Social")
           .SetUuid(social_folder_uuid)
           .SetChildren({UrlBuilder(u"LinkedIn", GURL("http://linkedin.com"))
                             .SetUuid(linkedin_uuid),
                         UrlBuilder(u"Twitter", GURL("http://twitter.com"))
                             .SetUuid(twitter_uuid)}),
       FolderBuilder(u"Shopping")
           .SetUuid(shopping_folder_uuid)
           .SetChildren(
               {UrlBuilder(u"Amazon", GURL("http://amazon.com"))
                    .SetUuid(amazon_uuid),
                UrlBuilder(u"eBay", GURL("http://ebay.com")).SetUuid(ebay_uuid),
                UrlBuilder(u"Etsy", GURL("http://etsy.com"))}),
       FolderBuilder(u"News")
           .SetUuid(news_folder_uuid)
           .SetChildren(
               {UrlBuilder(u"NYT", GURL("http://nyt.com")).SetUuid(nyt_uuid)}),
       FolderBuilder(u"Finance")
           .SetChildren({UrlBuilder(u"Bank", GURL("http://bank.com"))}),
       UrlBuilder(u"Email", GURL("http://email.com")).SetUuid(email_uuid),
       UrlBuilder(u"Maps", GURL("http://maps.com"))});

  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  ASSERT_THAT(account_bar->children(), SizeIs(4));
  ASSERT_THAT(local_bar->children(), SizeIs(6));

  bookmark_deduplicator_->Deduplicate();

  // Social folder (local) is identical to account -> removed.
  // News folder (local) is a subset of account -> removed.
  // Email bookmark (local) is identical to account -> removed.
  // Shopping folder (local) is a superset of account -> kept.
  // Finance folder (local) is unique -> kept.
  // Maps bookmark (local) is unique -> kept.
  //
  // 3. Local Bookmarks (AFTER Deduplication)
  // 🗂️ Bookmarks Bar (Local)
  // ├── 📁 Shopping
  // │   ├── 🔖 Amazon
  // │   ├── 🔖 eBay
  // │   └── 🔖 Etsy
  // ├── 📁 Finance
  // │   └── 🔖 Bank
  // └── 🔖 Maps
  EXPECT_THAT(local_bar->children(), SizeIs(3));
  EXPECT_EQ(u"Shopping", local_bar->children()[0]->GetTitle());
  EXPECT_EQ(u"Finance", local_bar->children()[1]->GetTitle());
  EXPECT_EQ(u"Maps", local_bar->children()[2]->GetTitle());

  // Children of the kept local folders should be untouched.
  EXPECT_THAT(local_bar->children()[0]->children(), SizeIs(3));
  EXPECT_THAT(local_bar->children()[1]->children(), SizeIs(1));

  // Account bookmarks should be untouched.
  EXPECT_THAT(account_bar->children(), SizeIs(4));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldNotRemoveIfUuidMatchesButTypeIsDifferentUrlVsFolder) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const std::u16string title = u"Title";

  AddLocalNodes({UrlBuilder(title, GURL("http://example.com")).SetUuid(uuid)});
  AddAccountNodes({FolderBuilder(title).SetUuid(uuid)});

  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  ASSERT_THAT(local_bar->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children(), SizeIs(1));

  bookmark_deduplicator_->Deduplicate();

  // No changes should have been made.
  EXPECT_THAT(local_bar->children(), SizeIs(1));
  EXPECT_THAT(account_bar->children(), SizeIs(1));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldNotRemoveIfUuidMatchesButTypeIsDifferentFolderVsUrl) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const std::u16string title = u"Title";

  AddLocalNodes({FolderBuilder(title).SetUuid(uuid)});
  AddAccountNodes(
      {UrlBuilder(title, GURL("http://example.com")).SetUuid(uuid)});

  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  ASSERT_THAT(local_bar->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children(), SizeIs(1));

  bookmark_deduplicator_->Deduplicate();

  // No changes should have been made.
  EXPECT_THAT(local_bar->children(), SizeIs(1));
  EXPECT_THAT(account_bar->children(), SizeIs(1));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldNotRemoveIfUrlUuidMatchesButUrlIsDifferent) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const std::u16string title = u"Title";

  AddLocalNodes({UrlBuilder(title, GURL("http://local.com")).SetUuid(uuid)});
  AddAccountNodes(
      {UrlBuilder(title, GURL("http://account.com")).SetUuid(uuid)});

  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  ASSERT_THAT(local_bar->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children(), SizeIs(1));

  bookmark_deduplicator_->Deduplicate();

  // No changes should have been made.
  EXPECT_THAT(local_bar->children(), SizeIs(1));
  EXPECT_THAT(account_bar->children(), SizeIs(1));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldNotRemoveIfFolderUuidMatchesButTitleIsDifferent) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();

  AddLocalNodes({FolderBuilder(u"Local Title").SetUuid(uuid)});
  AddAccountNodes({FolderBuilder(u"Account Title").SetUuid(uuid)});

  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  ASSERT_THAT(local_bar->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children(), SizeIs(1));

  bookmark_deduplicator_->Deduplicate();

  // No changes should have been made.
  EXPECT_THAT(local_bar->children(), SizeIs(1));
  EXPECT_THAT(account_bar->children(), SizeIs(1));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldRemoveDuplicateEmptyFolder) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const std::u16string title = u"Empty Folder";

  AddLocalNodes({FolderBuilder(title).SetUuid(uuid)});
  AddAccountNodes({FolderBuilder(title).SetUuid(uuid)});

  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  ASSERT_THAT(local_bar->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children(), SizeIs(1));

  bookmark_deduplicator_->Deduplicate();

  // The local folder should have been removed.
  EXPECT_THAT(local_bar->children(), IsEmpty());
  EXPECT_THAT(account_bar->children(), SizeIs(1));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldRemoveDuplicatesUnderOtherAndMobileNodes) {
  const base::Uuid other_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid mobile_uuid = base::Uuid::GenerateRandomV4();
  const GURL url("http://example.com");
  const std::u16string title = u"Title";

  FolderBuilder::AddChildrenTo(bookmark_model_.get(),
                               bookmark_model_->other_node(),
                               {UrlBuilder(title, url).SetUuid(other_uuid)});
  FolderBuilder::AddChildrenTo(bookmark_model_.get(),
                               bookmark_model_->account_other_node(),
                               {UrlBuilder(title, url).SetUuid(other_uuid)});

  FolderBuilder::AddChildrenTo(bookmark_model_.get(),
                               bookmark_model_->mobile_node(),
                               {UrlBuilder(title, url).SetUuid(mobile_uuid)});
  FolderBuilder::AddChildrenTo(bookmark_model_.get(),
                               bookmark_model_->account_mobile_node(),
                               {UrlBuilder(title, url).SetUuid(mobile_uuid)});

  ASSERT_THAT(bookmark_model_->other_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->account_other_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->mobile_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->account_mobile_node()->children(), SizeIs(1));

  bookmark_deduplicator_->Deduplicate();

  // The local bookmarks should have been removed.
  EXPECT_THAT(bookmark_model_->other_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model_->mobile_node()->children(), IsEmpty());
  EXPECT_THAT(bookmark_model_->account_other_node()->children(), SizeIs(1));
  EXPECT_THAT(bookmark_model_->account_mobile_node()->children(), SizeIs(1));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldNotRemoveIfAccountNodeHasDifferentParent) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const GURL url("http://example.com");
  const std::u16string title = u"Title";

  AddLocalNodes({UrlBuilder(title, url).SetUuid(uuid)});

  // Add the "duplicate" to a different folder in the account model.
  const bookmarks::BookmarkNode* account_folder = bookmark_model_->AddFolder(
      bookmark_model_->account_bookmark_bar_node(), 0, u"Some Folder");
  FolderBuilder::AddChildrenTo(bookmark_model_.get(), account_folder,
                               {UrlBuilder(title, url).SetUuid(uuid)});

  const bookmarks::BookmarkNode* local_bar =
      bookmark_model_->bookmark_bar_node();
  const bookmarks::BookmarkNode* account_bar =
      bookmark_model_->account_bookmark_bar_node();
  ASSERT_THAT(local_bar->children(), SizeIs(1));
  ASSERT_THAT(account_bar->children(), SizeIs(1));

  bookmark_deduplicator_->Deduplicate();

  // No changes should have been made because the parent doesn't match.
  EXPECT_THAT(local_bar->children(), SizeIs(1));
  EXPECT_THAT(account_bar->children(), SizeIs(1));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       DoesAccountSubgraphContainLocalSubgraphWithIdenticalUrls) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const GURL url("http://example.com");
  const std::u16string title = u"Title";

  AddLocalNodes({UrlBuilder(title, url).SetUuid(uuid)});
  AddAccountNodes({UrlBuilder(title, url).SetUuid(uuid)});

  ASSERT_THAT(bookmark_model_->bookmark_bar_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->account_bookmark_bar_node()->children(),
              SizeIs(1));

  const bookmarks::BookmarkNode* local_node =
      bookmark_model_->bookmark_bar_node()->children()[0].get();
  const bookmarks::BookmarkNode* account_node =
      bookmark_model_->account_bookmark_bar_node()->children()[0].get();

  EXPECT_TRUE(
      bookmark_deduplicator_->DoesAccountSubgraphContainLocalSubgraphForTest(
          local_node, account_node));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       DoesAccountSubgraphContainLocalSubgraphWithDifferentUrls) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const std::u16string title = u"Title";

  AddLocalNodes({UrlBuilder(title, GURL("http://local.com")).SetUuid(uuid)});
  AddAccountNodes(
      {UrlBuilder(title, GURL("http://account.com")).SetUuid(uuid)});

  ASSERT_THAT(bookmark_model_->bookmark_bar_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->account_bookmark_bar_node()->children(),
              SizeIs(1));

  const bookmarks::BookmarkNode* local_node =
      bookmark_model_->bookmark_bar_node()->children()[0].get();
  const bookmarks::BookmarkNode* account_node =
      bookmark_model_->account_bookmark_bar_node()->children()[0].get();

  EXPECT_FALSE(
      bookmark_deduplicator_->DoesAccountSubgraphContainLocalSubgraphForTest(
          local_node, account_node));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       DoesAccountSubgraphContainLocalSubgraphWithDifferentTypes) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const std::u16string title = u"Title";

  AddLocalNodes({UrlBuilder(title, GURL("http://example.com")).SetUuid(uuid)});
  AddAccountNodes({FolderBuilder(title).SetUuid(uuid)});

  ASSERT_THAT(bookmark_model_->bookmark_bar_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->account_bookmark_bar_node()->children(),
              SizeIs(1));

  const bookmarks::BookmarkNode* local_node =
      bookmark_model_->bookmark_bar_node()->children()[0].get();
  const bookmarks::BookmarkNode* account_node =
      bookmark_model_->account_bookmark_bar_node()->children()[0].get();

  EXPECT_FALSE(
      bookmark_deduplicator_->DoesAccountSubgraphContainLocalSubgraphForTest(
          local_node, account_node));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       DoesAccountSubgraphContainLocalSubgraphWithIdenticalFolders) {
  const base::Uuid folder_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid child_uuid = base::Uuid::GenerateRandomV4();
  const GURL child_url("http://child.com");
  const std::u16string folder_title = u"Folder";
  const std::u16string child_title = u"Child";

  AddLocalNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren(
               {UrlBuilder(child_title, child_url).SetUuid(child_uuid)})});
  AddAccountNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren(
               {UrlBuilder(child_title, child_url).SetUuid(child_uuid)})});

  ASSERT_THAT(bookmark_model_->bookmark_bar_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->account_bookmark_bar_node()->children(),
              SizeIs(1));

  const bookmarks::BookmarkNode* local_node =
      bookmark_model_->bookmark_bar_node()->children()[0].get();
  const bookmarks::BookmarkNode* account_node =
      bookmark_model_->account_bookmark_bar_node()->children()[0].get();

  EXPECT_TRUE(
      bookmark_deduplicator_->DoesAccountSubgraphContainLocalSubgraphForTest(
          local_node, account_node));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       DoesAccountSubgraphContainLocalSubgraphWithIdenticalEmptyFolders) {
  const base::Uuid folder_uuid = base::Uuid::GenerateRandomV4();
  const std::u16string folder_title = u"Folder";

  AddLocalNodes({FolderBuilder(folder_title).SetUuid(folder_uuid)});
  AddAccountNodes({FolderBuilder(folder_title).SetUuid(folder_uuid)});

  ASSERT_THAT(bookmark_model_->bookmark_bar_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->account_bookmark_bar_node()->children(),
              SizeIs(1));

  const bookmarks::BookmarkNode* local_node =
      bookmark_model_->bookmark_bar_node()->children()[0].get();
  const bookmarks::BookmarkNode* account_node =
      bookmark_model_->account_bookmark_bar_node()->children()[0].get();

  EXPECT_TRUE(
      bookmark_deduplicator_->DoesAccountSubgraphContainLocalSubgraphForTest(
          local_node, account_node));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       DoesAccountSubgraphContainLocalSubgraphWhenLocalIsSubgraph) {
  const base::Uuid folder_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid child_uuid = base::Uuid::GenerateRandomV4();
  const GURL child_url("http://child.com");
  const std::u16string folder_title = u"Folder";
  const std::u16string child_title = u"Child";

  AddLocalNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren(
               {UrlBuilder(child_title, child_url).SetUuid(child_uuid)})});
  AddAccountNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren({UrlBuilder(child_title, child_url).SetUuid(child_uuid),
                         UrlBuilder(u"Extra", GURL("http://extra.com"))})});

  ASSERT_THAT(bookmark_model_->bookmark_bar_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->account_bookmark_bar_node()->children(),
              SizeIs(1));

  const bookmarks::BookmarkNode* local_node =
      bookmark_model_->bookmark_bar_node()->children()[0].get();
  const bookmarks::BookmarkNode* account_node =
      bookmark_model_->account_bookmark_bar_node()->children()[0].get();

  EXPECT_TRUE(
      bookmark_deduplicator_->DoesAccountSubgraphContainLocalSubgraphForTest(
          local_node, account_node));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       DoesAccountSubgraphContainLocalSubgraphWhenAccountIsSubgraph) {
  const base::Uuid folder_uuid = base::Uuid::GenerateRandomV4();
  const base::Uuid child_uuid = base::Uuid::GenerateRandomV4();
  const GURL child_url("http://child.com");
  const std::u16string folder_title = u"Folder";
  const std::u16string child_title = u"Child";

  AddLocalNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren({UrlBuilder(child_title, child_url).SetUuid(child_uuid),
                         UrlBuilder(u"Extra", GURL("http://extra.com"))})});
  AddAccountNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren(
               {UrlBuilder(child_title, child_url).SetUuid(child_uuid)})});

  ASSERT_THAT(bookmark_model_->bookmark_bar_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->account_bookmark_bar_node()->children(),
              SizeIs(1));

  const bookmarks::BookmarkNode* local_node =
      bookmark_model_->bookmark_bar_node()->children()[0].get();
  const bookmarks::BookmarkNode* account_node =
      bookmark_model_->account_bookmark_bar_node()->children()[0].get();

  EXPECT_FALSE(
      bookmark_deduplicator_->DoesAccountSubgraphContainLocalSubgraphForTest(
          local_node, account_node));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       DoesAccountSubgraphContainLocalSubgraphWithDifferentFolderTitles) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();

  AddLocalNodes({FolderBuilder(u"Local Title").SetUuid(uuid)});
  AddAccountNodes({FolderBuilder(u"Account Title").SetUuid(uuid)});

  ASSERT_THAT(bookmark_model_->bookmark_bar_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->account_bookmark_bar_node()->children(),
              SizeIs(1));

  const bookmarks::BookmarkNode* local_node =
      bookmark_model_->bookmark_bar_node()->children()[0].get();
  const bookmarks::BookmarkNode* account_node =
      bookmark_model_->account_bookmark_bar_node()->children()[0].get();

  EXPECT_FALSE(
      bookmark_deduplicator_->DoesAccountSubgraphContainLocalSubgraphForTest(
          local_node, account_node));
}

TEST_F(InitialAccountBookmarkDeduplicatorTest,
       DoesAccountSubgraphContainLocalSubgraphWithDifferentChildUuid) {
  const base::Uuid folder_uuid = base::Uuid::GenerateRandomV4();
  const GURL child_url("http://child.com");
  const std::u16string folder_title = u"Folder";
  const std::u16string child_title = u"Child";

  AddLocalNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren({UrlBuilder(child_title, child_url)
                             .SetUuid(base::Uuid::GenerateRandomV4())})});
  AddAccountNodes(
      {FolderBuilder(folder_title)
           .SetUuid(folder_uuid)
           .SetChildren({UrlBuilder(child_title, child_url)
                             .SetUuid(base::Uuid::GenerateRandomV4())})});

  ASSERT_THAT(bookmark_model_->bookmark_bar_node()->children(), SizeIs(1));
  ASSERT_THAT(bookmark_model_->account_bookmark_bar_node()->children(),
              SizeIs(1));

  const bookmarks::BookmarkNode* local_node =
      bookmark_model_->bookmark_bar_node()->children()[0].get();
  const bookmarks::BookmarkNode* account_node =
      bookmark_model_->account_bookmark_bar_node()->children()[0].get();

  EXPECT_FALSE(
      bookmark_deduplicator_->DoesAccountSubgraphContainLocalSubgraphForTest(
          local_node, account_node));
}

// Verifies that `Deduplicate()` wraps duplicate node removals in an extensive
// changes scope on `BookmarkModel` so `BookmarkUndoService` suspends undo
// tracking while duplicates are removed.
TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldWrapDuplicateRemovalInExtensiveChanges) {
  const base::Uuid uuid = base::Uuid::GenerateRandomV4();
  const GURL url("http://example.com");
  const std::u16string title = u"Title";

  AddLocalNodes({UrlBuilder(title, url).SetUuid(uuid)});
  AddAccountNodes({UrlBuilder(title, url).SetUuid(uuid)});

  NiceMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(bookmark_model_.get());

  // `IsDoingExtensiveChanges()` must be true when `BookmarkNodeRemoved()` is
  // notified so `BookmarkUndoService` ignores the removal.
  {
    InSequence seq;
    EXPECT_CALL(observer, ExtensiveBookmarkChangesBeginning());
    EXPECT_CALL(observer, BookmarkNodeRemoved(_, _, _, _, _)).WillOnce([&]() {
      EXPECT_TRUE(bookmark_model_->IsDoingExtensiveChanges());
    });
    EXPECT_CALL(observer, ExtensiveBookmarkChangesEnded());
  }

  EXPECT_FALSE(bookmark_model_->IsDoingExtensiveChanges());
  bookmark_deduplicator_->Deduplicate();
  EXPECT_FALSE(bookmark_model_->IsDoingExtensiveChanges());
}

// Verifies that `Deduplicate()` balances `BeginExtensiveChanges()` and
// `EndExtensiveChanges()` even when no duplicate bookmarks exist.
TEST_F(InitialAccountBookmarkDeduplicatorTest,
       ShouldBalanceExtensiveChangesWhenNoDuplicatesExist) {
  AddLocalNodes({UrlBuilder(u"Local", GURL("http://local.com"))});
  AddAccountNodes({UrlBuilder(u"Account", GURL("http://account.com"))});

  NiceMock<MockBookmarkModelObserver> observer;
  base::ScopedObservation<BookmarkModel, BookmarkModelObserver> observation(
      &observer);
  observation.Observe(bookmark_model_.get());

  {
    InSequence seq;
    EXPECT_CALL(observer, ExtensiveBookmarkChangesBeginning());
    EXPECT_CALL(observer, BookmarkNodeRemoved(_, _, _, _, _)).Times(0);
    EXPECT_CALL(observer, ExtensiveBookmarkChangesEnded());
  }

  EXPECT_FALSE(bookmark_model_->IsDoingExtensiveChanges());
  bookmark_deduplicator_->Deduplicate();
  EXPECT_FALSE(bookmark_model_->IsDoingExtensiveChanges());
}

}  // namespace
}  // namespace sync_bookmarks
