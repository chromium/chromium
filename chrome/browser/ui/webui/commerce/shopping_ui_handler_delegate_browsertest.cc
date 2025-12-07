// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"

#include "base/json/json_reader.h"
#include "base/uuid.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

namespace {
const char kExampleUrl[] = "https://example.com/";
}  // namespace

// Tests ShoppingUiHandlerDelegate.
class ShoppingUiHandlerDelegateBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    profile_ = Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    signin::ConsentLevel consent_level =
        base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
            ? signin::ConsentLevel::kSignin
            : signin::ConsentLevel::kSync;
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_), "test@email.com",
        consent_level);

    bookmark_model_ = BookmarkModelFactory::GetForBrowserContext(profile_);
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);

    // If we are not syncing, we need to add account nodes in order to use the
    // handler. If we are, we need to pretend sync is on for bookmarks.
    if (consent_level == signin::ConsentLevel::kSignin) {
      bookmark_model_->CreateAccountPermanentFolders();
    } else {
      LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(
          browser()->profile())
          ->SetIsTrackingMetadataForTesting();
    }
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateToURL(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    base::PlatformThread::Sleep(base::Seconds(2));
    base::RunLoop().RunUntilIdle();
  }

  void OpenURLInNewTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
            ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    base::PlatformThread::Sleep(base::Seconds(2));
    base::RunLoop().RunUntilIdle();
  }

  raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<bookmarks::BookmarkModel, DanglingUntriaged> bookmark_model_;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
#endif
};

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestGetCurrentUrl) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(profile_);
  const GURL url = GURL(kExampleUrl);
  NavigateToURL(url);

  ASSERT_TRUE(delegate->GetCurrentTabUrl().has_value());
  ASSERT_EQ(delegate->GetCurrentTabUrl().value(), url);
}

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestGetBookmarkForCurrentUrl) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(profile_);
  const GURL url = GURL(kExampleUrl);
  NavigateToURL(url);

  const bookmarks::BookmarkNode* parent =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? bookmark_model_->account_other_node()
          : bookmark_model_->other_node();
  auto* existing_node = bookmark_model_->AddNewURL(
      parent, parent->children().size(), u"test", url);
  size_t bookmark_count = parent->children().size();

  auto* node = delegate->GetOrAddBookmarkForCurrentUrl();
  ASSERT_EQ(existing_node->id(), node->id());
  ASSERT_EQ(bookmark_count, parent->children().size());
}

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestAddBookmarkForCurrentUrl) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(profile_);
  const GURL url = GURL(kExampleUrl);
  NavigateToURL(url);

  const bookmarks::BookmarkNode* parent =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? bookmark_model_->account_other_node()
          : bookmark_model_->other_node();
  size_t bookmark_count = parent->children().size();

  auto* node = delegate->GetOrAddBookmarkForCurrentUrl();

  DCHECK(node);
  ASSERT_EQ(bookmark_count + 1, parent->children().size());
}

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestSwitchToOrOpenTab_SwitchToExistingTab) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(profile_);
  const GURL url_1 = GURL(kExampleUrl);
  NavigateToURL(url_1);
  const auto* web_contents_1 = web_contents();
  const GURL url_2 = GURL("https://www.google.com");
  OpenURLInNewTab(url_2);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_NE(web_contents(), web_contents_1);
  ASSERT_EQ(url_2, web_contents()->GetLastCommittedURL());
  delegate->SwitchToOrOpenTab(url_1);

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(web_contents_1, web_contents());
  EXPECT_EQ(url_1, web_contents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestSwitchToOrOpenTab_OpenNewTab) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(profile_);
  const GURL url = GURL(kExampleUrl);
  NavigateToURL(url);
  const GURL url_2 = GURL("https://www.google.com");
  content::TestNavigationObserver observer(url_2);
  observer.StartWatchingNewWebContents();

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(url, web_contents()->GetLastCommittedURL());
  delegate->SwitchToOrOpenTab(url_2);
  observer.WaitForNavigationFinished();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(url_2, web_contents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestSwitchToOrOpenTab_InvalidUrls) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(profile_);
  const GURL invalid_url_1 = GURL("chrome://newtab");
  NavigateToURL(invalid_url_1);
  const GURL invalid_url_2 = GURL("file://foo");
  OpenURLInNewTab(invalid_url_2);
  const GURL valid_url = GURL(kExampleUrl);
  OpenURLInNewTab(valid_url);
  const auto* valid_web_contents = web_contents();

  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_EQ(valid_web_contents, web_contents());
  ASSERT_EQ(valid_url, web_contents()->GetLastCommittedURL());
  delegate->SwitchToOrOpenTab(invalid_url_1);

  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  // Ensure that the web contents remain the same, since `SwitchToOrOpenTab`
  // shouldn't work for non-HTTP(S) urls.
  EXPECT_EQ(valid_web_contents, web_contents());
  EXPECT_EQ(valid_url, web_contents()->GetLastCommittedURL());

  delegate->SwitchToOrOpenTab(invalid_url_2);

  EXPECT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(valid_web_contents, web_contents());
  EXPECT_EQ(valid_url, web_contents()->GetLastCommittedURL());
}

// The feedback dialog on CrOS happens at the system level, which cannot be
// easily tested here.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestShowFeedbackForProductSpecifications) {
  const std::string log_id = "test_id";
  ASSERT_EQ(nullptr, FeedbackDialog::GetInstanceForTest());

  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(profile_);
  delegate->ShowFeedbackForProductSpecifications(log_id);

  // Feedback dialog should be non-null with correct meta data.
  CHECK(FeedbackDialog::GetInstanceForTest());
  EXPECT_EQ(chrome::kChromeUIFeedbackURL,
            FeedbackDialog::GetInstanceForTest()->GetDialogContentURL());
  std::optional<base::Value::Dict> meta_data = base::JSONReader::ReadDict(
      FeedbackDialog::GetInstanceForTest()->GetDialogArgs(),
      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(meta_data.has_value());
  ASSERT_EQ(*meta_data->FindString("categoryTag"), "compare");
  std::optional<base::Value::Dict> ai_meta_data =
      base::JSONReader::ReadDict(*meta_data->FindString("aiMetadata"),
                                 base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(ai_meta_data.has_value());
  ASSERT_EQ(*ai_meta_data->FindString("log_id"), log_id);
}

// When the user has the page saved locally, an account node is created instead
// so that the feature can be used.
IN_PROC_BROWSER_TEST_F(ShoppingUiHandlerDelegateBrowserTest,
                       TestGetLocalBookmarkCreatesNewAccountBookmark) {
  auto delegate =
      std::make_unique<commerce::ShoppingUiHandlerDelegate>(profile_);
  const GURL url = GURL(kExampleUrl);
  NavigateToURL(url);

  auto* existing_node = bookmark_model_->AddNewURL(
      bookmark_model_->other_node(),
      bookmark_model_->other_node()->children().size(), u"test", url);
  size_t account_bookmark_count =
      bookmark_model_->account_other_node()->children().size();

  auto* node = delegate->GetOrAddBookmarkForCurrentUrl();
  EXPECT_NE(existing_node->id(), node->id());
  EXPECT_EQ(account_bookmark_count + 1,
            bookmark_model_->account_other_node()->children().size());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
