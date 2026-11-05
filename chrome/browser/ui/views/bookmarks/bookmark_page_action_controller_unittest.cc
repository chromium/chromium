// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_page_action_controller.h"

#include <optional>
#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;
using testing::ReturnRef;

class BookmarkPageActionControllerTest : public testing::Test {
 public:
  BookmarkPageActionControllerTest() {
    BookmarkModelFactory::GetInstance()->SetTestingFactory(
        &profile_, BookmarkModelFactory::GetDefaultFactory());
    bookmarks::test::WaitForBookmarkModelToLoad(
        BookmarkModelFactory::GetForBrowserContext(&profile_));

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    BookmarkTabHelper::CreateForWebContents(web_contents_.get());
    ON_CALL(tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(tab_interface_, GetContents())
        .WillByDefault(Return(web_contents_.get()));

    pref_service_.registry()->RegisterBooleanPref(
        bookmarks::prefs::kEditBookmarksEnabled, false);

    bookmark_page_action_controller_ =
        std::make_unique<BookmarkPageActionController>(
            tab_interface_, &pref_service_, page_action_controller_);
  }

  void SetUp() override {
    content::WebContentsTester::For(web_contents())
        ->SetLastCommittedURL(GURL("https://www.example.com"));
  }

  BookmarkPageActionControllerTest(const BookmarkPageActionControllerTest&) =
      delete;
  BookmarkPageActionControllerTest& operator=(
      const BookmarkPageActionControllerTest&) = delete;

  ~BookmarkPageActionControllerTest() override = default;

  page_actions::MockPageActionController& page_action_controller() {
    return page_action_controller_;
  }

  BookmarkPageActionController& bookmark_page_action_controller() {
    return *bookmark_page_action_controller_.get();
  }

  TestingPrefServiceSimple& pref_service() { return pref_service_; }

  tabs::MockTabInterface& tab() { return tab_interface_; }

  TestingProfile& profile() { return profile_; }

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  tabs::MockTabInterface tab_interface_;
  TestingProfile profile_;
  TestingPrefServiceSimple pref_service_;
  page_actions::MockPageActionController page_action_controller_;
  ui::UnownedUserDataHost user_data_host_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<BookmarkPageActionController>
      bookmark_page_action_controller_;
};

TEST_F(BookmarkPageActionControllerTest, URLStarredChangedUpdatesImageAndName) {
  EXPECT_CALL(page_action_controller(),
              OverrideAccessibleName(kActionBookmarkThisTab, _))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionBookmarkThisTab, _))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              OverrideImage(kActionBookmarkThisTab, _, _, _))
      .Times(1);

  bookmark_page_action_controller().URLStarredChanged(
      tab().GetContents(), /*starred=*/false,
      /*is_bookmark_added_or_removed=*/false);

  EXPECT_CALL(page_action_controller(),
              OverrideAccessibleName(kActionBookmarkThisTab, _))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              OverrideTooltip(kActionBookmarkThisTab, _))
      .Times(1);
  EXPECT_CALL(page_action_controller(),
              OverrideImage(kActionBookmarkThisTab, _, _, _))
      .Times(1);

  bookmark_page_action_controller().URLStarredChanged(
      tab().GetContents(), /*starred=*/true,
      /*is_bookmark_added_or_removed=*/true);
}

TEST_F(BookmarkPageActionControllerTest, EditBookmarkPrefControlsVisibility) {
  EXPECT_CALL(page_action_controller(), Show(kActionBookmarkThisTab)).Times(1);
  pref_service().SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, true);

  EXPECT_CALL(page_action_controller(), Hide(kActionBookmarkThisTab)).Times(1);
  pref_service().SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
}

TEST_F(BookmarkPageActionControllerTest, HiddenOnNTP) {
  pref_service().SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, true);

  // Simulate a navigation to the given `destination_url`.
  auto navigate_to_url = [&](GURL destination_url) {
    content::WebContentsTester::For(web_contents())
        ->SetLastCommittedURL(destination_url);
    static_cast<content::WebContentsObserver*>(
        &bookmark_page_action_controller())
        ->PrimaryPageChanged(web_contents()->GetPrimaryPage());
  };

  // Shown on non-NTP page context.
  EXPECT_CALL(page_action_controller(), Show(kActionBookmarkThisTab)).Times(1);
  navigate_to_url(GURL("https://www.example.com"));

  // Hidden on NTP page context ("chrome://newtab/").
  EXPECT_CALL(page_action_controller(), Hide(kActionBookmarkThisTab)).Times(1);
  navigate_to_url(chrome::ChromeUINewTabURLAsGURL());

  // Shown on non-NTP page context.
  EXPECT_CALL(page_action_controller(), Show(kActionBookmarkThisTab)).Times(1);
  navigate_to_url(GURL("https://www.example.com"));

  // Hidden on NTP page context ("chrome://new-tab-page/").
  EXPECT_CALL(page_action_controller(), Hide(kActionBookmarkThisTab)).Times(1);
  navigate_to_url(chrome::ChromeUINewTabPageURLAsGURL());
}

TEST_F(BookmarkPageActionControllerTest,
       DoesNotAnimateOnNavigationBetweenBookmarkedAndUnbookmarkedPages) {
  base::test::ScopedFeatureList feature_list{features::kToolbarGlowUp};

  const GURL kBookmarkedUrl("https://www.google.com");
  const GURL kUnbookmarkedUrl("chrome://dino");

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(&profile());
  bookmark_model->AddNewURL(bookmark_model->bookmark_bar_node(), 0,
                            u"Bookmarked", kBookmarkedUrl);

  // Navigating from an unbookmarked page to a bookmarked page should update the
  // icon without animating.
  EXPECT_CALL(
      page_action_controller(),
      OverrideImage(
          kActionBookmarkThisTab, _, _,
          std::optional<page_actions::PageActionAnimationParams>(std::nullopt)))
      .Times(1);
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(kBookmarkedUrl);
  testing::Mock::VerifyAndClearExpectations(&page_action_controller());

  // Navigating away from a bookmarked page to an unbookmarked page should also
  // update the icon without animating.
  EXPECT_CALL(
      page_action_controller(),
      OverrideImage(
          kActionBookmarkThisTab, _, _,
          std::optional<page_actions::PageActionAnimationParams>(std::nullopt)))
      .Times(1);
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(kUnbookmarkedUrl);
}
