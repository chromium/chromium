// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

class BookmarkMenuCocoaControllerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    menu_ = [[NSMenu alloc] initWithTitle:@"test"];
  }

  void TearDown() override {
    bridge_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
                BookmarkModelFactory::GetInstance(),
                BookmarkModelFactory::GetDefaultFactory()},
            TestingProfile::TestingFactory{
                ManagedBookmarkServiceFactory::GetInstance(),
                ManagedBookmarkServiceFactory::GetDefaultFactory()},
            TestingProfile::TestingFactory{
                BookmarkMergedSurfaceServiceFactory::GetInstance(),
                BookmarkMergedSurfaceServiceFactory::GetDefaultFactory()}};
  }

  void InitBridgeAndController() {
    bridge_ = std::make_unique<BookmarkMenuBridge>(profile(), menu_);
    controller_ =
        [[BookmarkMenuCocoaController alloc] initWithBridge:bridge_.get()];
  }

  BookmarkMergedSurfaceService* bookmark_service() {
    return BookmarkMergedSurfaceServiceFactory::GetForProfile(profile());
  }

  BookmarkModel* model() { return bookmark_service()->bookmark_model(); }

  BookmarkMenuCocoaController* controller() { return controller_; }
  BookmarkMenuBridge* bridge() { return bridge_.get(); }
  NSMenu* menu() { return menu_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kSyncEnableBookmarksInTransportMode};
  CocoaTestHelper cocoa_test_helper_;
  NSMenu* __strong menu_;
  std::unique_ptr<BookmarkMenuBridge> bridge_;
  BookmarkMenuCocoaController* __strong controller_;
};

TEST_F(BookmarkMenuCocoaControllerTest, TestOpenItemAfterModelLoaded) {
  const GURL kUrl1("http://site1.com");
  const GURL kUrl2("http://site2.com");
  const GURL kUrl3("http://site3.com");
  const GURL kUrl4("http://site4.com");

  WaitForBookmarkMergedSurfaceServiceToLoad(bookmark_service());
  ASSERT_TRUE(bookmark_service()->loaded());
  model()->CreateAccountPermanentFolders();

  const BookmarkNode* node1 =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"", kUrl1);
  const BookmarkNode* node2 =
      model()->AddURL(model()->account_bookmark_bar_node(), 0, u"", kUrl2);
  const BookmarkNode* node3 =
      model()->AddURL(model()->other_node(), 0, u"", kUrl3);
  const BookmarkNode* node4 =
      model()->AddURL(model()->account_other_node(), 0, u"", kUrl4);

  InitBridgeAndController();

  AddTab(browser(), GURL("about:blank"));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  CHECK(contents);

  content::TestNavigationObserver navigation_observer(contents);

  BookmarkMenuCocoaController* c = controller();

  // Populate the root menu.
  [c menuNeedsUpdate:menu()];
  // Populate the submenu for `Other Bookmarks`.
  NSMenuItem* other_item =
      [menu() itemWithTag:model()->account_other_node()->id()];
  ASSERT_TRUE([other_item hasSubmenu]);
  [c menuNeedsUpdate:[other_item submenu]];

  NSMenuItem* item1 = bridge()->MenuItemForNodeForTest(node1);
  NSMenuItem* item2 = bridge()->MenuItemForNodeForTest(node2);
  NSMenuItem* item3 = bridge()->MenuItemForNodeForTest(node3);
  NSMenuItem* item4 = bridge()->MenuItemForNodeForTest(node4);
  ASSERT_NE(nullptr, item1);
  ASSERT_NE(nullptr, item2);
  ASSERT_NE(nullptr, item3);
  ASSERT_NE(nullptr, item4);

  ASSERT_EQ(navigation_observer.last_navigation_url(), GURL());

  base::UserActionTester user_actions;

  [c openBookmarkMenuItem:item1];

  CommitPendingLoad(&contents->GetController());
  navigation_observer.WaitForNavigationFinished();
  EXPECT_EQ(navigation_observer.last_navigation_url(), kUrl1);
  EXPECT_EQ(1, user_actions.GetActionCount("TopMenu_Bookmarks_LaunchURL"));

  [c openBookmarkMenuItem:item2];

  CommitPendingLoad(&contents->GetController());
  navigation_observer.WaitForNavigationFinished();
  EXPECT_EQ(navigation_observer.last_navigation_url(), kUrl2);
  EXPECT_EQ(2, user_actions.GetActionCount("TopMenu_Bookmarks_LaunchURL"));

  [c openBookmarkMenuItem:item3];

  CommitPendingLoad(&contents->GetController());
  navigation_observer.WaitForNavigationFinished();
  EXPECT_EQ(navigation_observer.last_navigation_url(), kUrl3);
  EXPECT_EQ(3, user_actions.GetActionCount("TopMenu_Bookmarks_LaunchURL"));

  [c openBookmarkMenuItem:item4];

  CommitPendingLoad(&contents->GetController());
  navigation_observer.WaitForNavigationFinished();
  EXPECT_EQ(navigation_observer.last_navigation_url(), kUrl4);
  EXPECT_EQ(4, user_actions.GetActionCount("TopMenu_Bookmarks_LaunchURL"));
}

TEST_F(BookmarkMenuCocoaControllerTest, TestOpenItemWhileModelLoading) {
  const GURL kUrl1("http://site1.com");
  const GURL kUrl2("http://site2.com");
  const GURL kUrl3("http://site3.com");
  const GURL kUrl4("http://site4.com");

  WaitForBookmarkMergedSurfaceServiceToLoad(bookmark_service());
  ASSERT_TRUE(bookmark_service()->loaded());
  model()->CreateAccountPermanentFolders();

  const BookmarkNode* node1 =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"", kUrl1);
  const BookmarkNode* node2 =
      model()->AddURL(model()->account_bookmark_bar_node(), 0, u"", kUrl2);
  const BookmarkNode* node3 =
      model()->AddURL(model()->other_node(), 0, u"", kUrl3);
  const BookmarkNode* node4 =
      model()->AddURL(model()->account_other_node(), 0, u"", kUrl4);

  const base::Uuid uuid1 = node1->uuid();
  const base::Uuid uuid2 = node2->uuid();
  const base::Uuid uuid3 = node3->uuid();
  const base::Uuid uuid4 = node4->uuid();

  // Ensure that the bookmarks JSON file is written to disk.
  model()->CommitPendingWriteForTest();
  task_environment()->RunUntilIdle();

  // Mimic a scenario where a new profile was created and
  // BookmarkMergedSurfaceService is in the process of loading.
  BookmarkMergedSurfaceServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        auto model = std::make_unique<BookmarkMergedSurfaceService>(
            BookmarkModelFactory::GetForBrowserContext(context),
            ManagedBookmarkServiceFactory::GetForProfile(
                Profile::FromBrowserContext(context)));
        model->Load(context->GetPath());
        return model;
      }));

  ASSERT_FALSE(bookmark_service()->loaded());

  AddTab(browser(), GURL("about:blank"));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  CHECK(contents);

  content::TestNavigationObserver navigation_observer(contents);

  ASSERT_EQ(navigation_observer.last_navigation_url(), GURL());

  base::UserActionTester user_actions;

  [BookmarkMenuCocoaController
      openBookmarkByGUID:uuid1
               inProfile:profile()
         withDisposition:WindowOpenDisposition::CURRENT_TAB];

  // While the model is loading, no navigation could have happened.
  EXPECT_EQ(nullptr, contents->GetController().GetPendingEntry());

  WaitForBookmarkMergedSurfaceServiceToLoad(bookmark_service());
  ASSERT_TRUE(bookmark_service()->loaded());
  ASSERT_NE(
      nullptr,
      model()->GetNodeByUuid(
          uuid1, BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  ASSERT_NE(nullptr,
            model()->GetNodeByUuid(
                uuid2, BookmarkModel::NodeTypeForUuidLookup::kAccountNodes));
  ASSERT_NE(
      nullptr,
      model()->GetNodeByUuid(
          uuid3, BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  ASSERT_NE(nullptr,
            model()->GetNodeByUuid(
                uuid4, BookmarkModel::NodeTypeForUuidLookup::kAccountNodes));

  // Once the model is loaded, the bookmark should open.
  CommitPendingLoad(&contents->GetController());
  navigation_observer.WaitForNavigationFinished();

  EXPECT_EQ(navigation_observer.last_navigation_url(), kUrl1);
  EXPECT_EQ(1, user_actions.GetActionCount("TopMenu_Bookmarks_LaunchURL"));
}
