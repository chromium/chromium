// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmarks_message_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/any_widget_observer.h"

class BookmarkMessageHandlerTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ResetWithProfile(browser()->profile());
    SignInAndEnableAccountBookmarkNodes(browser()->profile());
  }

  void TearDownOnMainThread() override {
    webui_contents_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  bool SendCanUploadBookmarkToAccountStorage(const std::string& id_string) {
    return handler_->CanUploadBookmarkToAccountStorage(id_string);
  }

  void SendHandleSingleUploadClicked(const std::string& id_string) {
    base::Value::List args;
    args.Append(id_string);
    handler_->HandleSingleUploadClicked(args);
  }

  void ResetWithProfile(Profile* profile) {
    webui_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile));
    web_ui_.set_web_contents(webui_contents_.get());

    auto handler_owner = std::make_unique<BookmarksMessageHandler>();
    handler_ = handler_owner.get();
    web_ui_.AddMessageHandler(std::move(handler_owner));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kSyncEnableBookmarksInTransportMode};

  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> webui_contents_;
  raw_ptr<BookmarksMessageHandler> handler_;
};

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest,
                       CanUploadLocalBookmarkWithAccountNodesPresent) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  ASSERT_TRUE(model->account_other_node());

  const bookmarks::BookmarkNode* node = model->AddURL(
      model->other_node(), 0, std::u16string(), GURL("http://test.com"));
  const std::string id_string = base::NumberToString(node->id());

  EXPECT_TRUE(SendCanUploadBookmarkToAccountStorage(id_string));
}

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest,
                       CanNotUploadWithEditDisabled) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kEditBookmarksEnabled, false);

  const bookmarks::BookmarkNode* node = model->AddURL(
      model->other_node(), 0, std::u16string(), GURL("http://test.com"));
  const std::string id_string = base::NumberToString(node->id());

  EXPECT_FALSE(SendCanUploadBookmarkToAccountStorage(id_string));
}

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest,
                       CanNotUploadWithInvalidBookmarkId) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  EXPECT_FALSE(SendCanUploadBookmarkToAccountStorage("test"));
}

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest,
                       CanNotUploadWithMissingBookmarkId) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  const bookmarks::BookmarkNode* node = model->AddURL(
      model->other_node(), 0, std::u16string(), GURL("http://test.com"));
  const std::string id_string = base::NumberToString(node->id());

  // Remove the bookmark so that `id_string` will point to a non-existing
  // bookmark.
  model->Remove(model->other_node()->children()[0].get(),
                bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

  EXPECT_FALSE(SendCanUploadBookmarkToAccountStorage(id_string));
}

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest, CanNotUploadPermanentNode) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  const std::string id_string = base::NumberToString(model->other_node()->id());

  EXPECT_FALSE(SendCanUploadBookmarkToAccountStorage(id_string));
}

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest,
                       CanNotUploadWithoutAccountNodesPresent) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  model->RemoveAccountPermanentFolders();
  ASSERT_FALSE(model->account_other_node());

  const bookmarks::BookmarkNode* node = model->AddURL(
      model->other_node(), 0, std::u16string(), GURL("http://test.com"));
  const std::string id_string = base::NumberToString(node->id());

  EXPECT_FALSE(SendCanUploadBookmarkToAccountStorage(id_string));
}

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest, CanNotUploadManagedNode) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  bookmarks::ManagedBookmarkService* managed_bookmark_service =
      ManagedBookmarkServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(managed_bookmark_service->managed_node());

  // Add a managed bookmark.
  const bookmarks::BookmarkNode* node =
      model->AddURL(managed_bookmark_service->managed_node(), 0,
                    std::u16string(), GURL("http://test.com"));
  const std::string id_string = base::NumberToString(node->id());

  ASSERT_TRUE(managed_bookmark_service->IsNodeManaged(node));
  EXPECT_FALSE(SendCanUploadBookmarkToAccountStorage(id_string));
}

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest,
                       CanNotUploadWithSyncEnabled) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  // Pretend sync is on for bookmarks.
  LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(browser()->profile())
      ->SetIsTrackingMetadataForTesting();

  const bookmarks::BookmarkNode* node = model->AddURL(
      model->other_node(), 0, std::u16string(), GURL("http://test.com"));
  const std::string id_string = base::NumberToString(node->id());

  EXPECT_FALSE(SendCanUploadBookmarkToAccountStorage(id_string));
}

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest,
                       CanNotUploadAccountBookmark) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  const bookmarks::BookmarkNode* node =
      model->AddURL(model->account_other_node(), 0, std::u16string(),
                    GURL("http://test.com"));
  const std::string id_string = base::NumberToString(node->id());

  EXPECT_FALSE(SendCanUploadBookmarkToAccountStorage(id_string));
}

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest,
                       CanNotUploadInIncognitoMode) {
  // Add a bookmark that can be uploaded.
  Profile* original_profile = browser()->profile();
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(original_profile);
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* node = model->AddURL(
      model->other_node(), 0, std::u16string(), GURL("http://test.com"));
  const std::string id_string = base::NumberToString(node->id());
  ASSERT_TRUE(SendCanUploadBookmarkToAccountStorage(id_string));

  // Simulate opening the webui in Incognito mode.
  Profile* otr_profile =
      original_profile->GetPrimaryOTRProfile(/*create_if_needed*/ true);
  ResetWithProfile(otr_profile);
  EXPECT_FALSE(SendCanUploadBookmarkToAccountStorage(id_string));
}

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest,
                       CanNotUploadInSigninPending) {
  // Add a bookmark that can be uploaded.
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* node = model->AddURL(
      model->other_node(), 0, std::u16string(), GURL("http://test.com"));
  const std::string id_string = base::NumberToString(node->id());
  ASSERT_TRUE(SendCanUploadBookmarkToAccountStorage(id_string));

  // Set Signin Pending state.
  signin::SetInvalidRefreshTokenForPrimaryAccount(
      IdentityManagerFactory::GetForProfile(browser()->profile()));

  EXPECT_FALSE(SendCanUploadBookmarkToAccountStorage(id_string));
}

IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest,
                       SingleUploadClickedOpensDialog) {
  // Add a bookmark that can be uploaded.
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  const bookmarks::BookmarkNode* node = model->AddURL(
      model->other_node(), 0, std::u16string(), GURL("http://test.com"));
  const std::string id_string = base::NumberToString(node->id());
  ASSERT_TRUE(SendCanUploadBookmarkToAccountStorage(id_string));

  // Click the single upload icon. This should open the dialog.
  views::NamedWidgetShownWaiter dialog_waiter(
      views::test::AnyWidgetTestPasskey{}, "BookmarkAccountStorageMoveDialog");
  SendHandleSingleUploadClicked(id_string);
  auto* upload_dialog = dialog_waiter.WaitIfNeededAndGet();
  EXPECT_NE(upload_dialog, nullptr);
}

// TODO(crbug.com/413637312): Remove this test once the icon is no longer shown
// upon signout.
IN_PROC_BROWSER_TEST_F(BookmarkMessageHandlerTest,
                       SingleUploadClickedDoesNotCrashWithoutAccountNodes) {
  // Add a bookmark that can be uploaded, but remove the account nodes (which
  // could happen e.g. if the user signs out).
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  model->RemoveAccountPermanentFolders();
  ASSERT_FALSE(model->account_other_node());
  const bookmarks::BookmarkNode* node = model->AddURL(
      model->other_node(), 0, std::u16string(), GURL("http://test.com"));
  const std::string id_string = base::NumberToString(node->id());
  ASSERT_FALSE(SendCanUploadBookmarkToAccountStorage(id_string));

  // Click the single upload icon. This should not crash.
  SendHandleSingleUploadClicked(id_string);
}
