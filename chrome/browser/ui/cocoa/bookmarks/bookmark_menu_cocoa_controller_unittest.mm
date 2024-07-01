// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_cocoa_controller.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/uuid.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
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
                ManagedBookmarkServiceFactory::GetDefaultFactory()}};
  }

  void InitBridgeAndController() {
    bridge_ = std::make_unique<BookmarkMenuBridge>(profile(), menu_);

    controller_ =
        [[BookmarkMenuCocoaController alloc] initWithBridge:bridge_.get()];
  }

  BookmarkModel* model() {
    return BookmarkModelFactory::GetForBrowserContext(profile());
  }

  BookmarkMenuCocoaController* controller() { return controller_; }
  BookmarkMenuBridge* bridge() { return bridge_.get(); }
  NSMenu* menu() { return menu_; }

 private:
  CocoaTestHelper cocoa_test_helper_;
  NSMenu* __strong menu_;
  std::unique_ptr<BookmarkMenuBridge> bridge_;
  BookmarkMenuCocoaController* __strong controller_;
};

TEST_F(BookmarkMenuCocoaControllerTest, TestOpenItemAfterModelLoaded) {
  const GURL kUrl1("http://site1.com");
  const GURL kUrl2("http://site2.com");

  bookmarks::test::WaitForBookmarkModelToLoad(model());
  ASSERT_TRUE(model()->loaded());

  const BookmarkNode* bookmark_bar = model()->bookmark_bar_node();
  const BookmarkNode* node1 = model()->AddURL(bookmark_bar, 0, u"", kUrl1);
  const BookmarkNode* node2 = model()->AddURL(bookmark_bar, 1, u"", kUrl2);

  InitBridgeAndController();

  AddTab(browser(), GURL("about:blank"));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  CHECK(contents);

  content::TestNavigationObserver navigation_observer(contents);

  BookmarkMenuCocoaController* c = controller();

  // Required to populate the bridge.
  [c menuNeedsUpdate:menu()];

  NSMenuItem* item1 = bridge()->MenuItemForNodeForTest(node1);
  NSMenuItem* item2 = bridge()->MenuItemForNodeForTest(node2);
  ASSERT_NE(nullptr, item1);
  ASSERT_NE(nullptr, item2);

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
}

TEST_F(BookmarkMenuCocoaControllerTest, TestOpenItemWhileModelLoading) {
  const GURL kUrl1("http://site1.com");
  const GURL kUrl2("http://site2.com");

  bookmarks::test::WaitForBookmarkModelToLoad(model());
  ASSERT_TRUE(model()->loaded());

  const BookmarkNode* bookmark_bar = model()->bookmark_bar_node();
  const BookmarkNode* node1 = model()->AddURL(bookmark_bar, 0, u"", kUrl1);
  const BookmarkNode* node2 = model()->AddURL(bookmark_bar, 1, u"", kUrl2);

  const base::Uuid uuid1 = node1->uuid();
  const base::Uuid uuid2 = node2->uuid();

  // Ensure that the bookmarks JSON file is written to disk.
  model()->CommitPendingWriteForTest();
  task_environment()->RunUntilIdle();

  // Mimic a scenario where a new profile was created and BookmarkModel is in
  // the process of loading.
  BookmarkModelFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
        auto model = std::make_unique<BookmarkModel>(
            std::make_unique<bookmarks::TestBookmarkClient>());
        model->Load(context->GetPath());
        return model;
      }));

  ASSERT_FALSE(model()->loaded());

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

  bookmarks::test::WaitForBookmarkModelToLoad(model());
  ASSERT_TRUE(model()->loaded());
  ASSERT_NE(
      nullptr,
      model()->GetNodeByUuid(
          uuid1, BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));
  ASSERT_NE(
      nullptr,
      model()->GetNodeByUuid(
          uuid2, BookmarkModel::NodeTypeForUuidLookup::kLocalOrSyncableNodes));

  // Once the model is loaded, the bookmark should open.
  CommitPendingLoad(&contents->GetController());
  navigation_observer.WaitForNavigationFinished();

  EXPECT_EQ(navigation_observer.last_navigation_url(), kUrl1);
  EXPECT_EQ(1, user_actions.GetActionCount("TopMenu_Bookmarks_LaunchURL"));
}
