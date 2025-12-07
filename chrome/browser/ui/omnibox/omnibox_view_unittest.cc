// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_view.h"

#include <stddef.h>

#include <array>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/test_omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/test_omnibox_popup_view.h"
#include "chrome/browser/ui/omnibox/test_omnibox_view.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_text_util.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

using base::ASCIIToUTF16;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;
using testing::SaveArgPointee;

namespace {

class OmniboxViewTest : public testing::Test {
 public:
  OmniboxViewTest()
      : bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()) {
    // Create the controller and the view and wire them together.
    auto omnibox_client = std::make_unique<TestOmniboxClient>();
    omnibox_client_ = omnibox_client.get();
    EXPECT_CALL(*client(), GetBookmarkModel())
        .WillRepeatedly(Return(bookmark_model_.get()));
    omnibox_controller_ =
        std::make_unique<OmniboxController>(std::move(omnibox_client));
    omnibox_controller_->SetEditModelForTesting(
        std::make_unique<TestOmniboxEditModel>(omnibox_controller_.get(),
                                               /*pref_service=*/nullptr));
    view_ = std::make_unique<TestOmniboxView>(omnibox_controller_.get());
  }

  TestOmniboxView* view() { return view_.get(); }

  TestOmniboxEditModel* model() {
    return static_cast<TestOmniboxEditModel*>(
        omnibox_controller_->edit_model());
  }

  OmniboxController* controller() { return omnibox_controller_.get(); }

  TestOmniboxClient* client() { return omnibox_client_; }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<OmniboxController> omnibox_controller_;
  std::unique_ptr<TestOmniboxView> view_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<TestOmniboxClient> omnibox_client_;
};

class OmniboxViewPopupTest : public testing::Test {
 public:
  OmniboxViewPopupTest() {
    // Create the controller and the view and wire them together.
    auto omnibox_client = std::make_unique<TestOmniboxClient>();
    omnibox_client_ = omnibox_client.get();
    omnibox_controller_ =
        std::make_unique<OmniboxController>(std::move(omnibox_client));
    omnibox_controller_->SetEditModelForTesting(
        std::make_unique<TestOmniboxEditModel>(omnibox_controller_.get(),
                                               /*pref_service=*/nullptr));
    view_ = std::make_unique<TestOmniboxView>(omnibox_controller_.get());

    model()->set_popup_view(&popup_view_);
    omnibox_controller_->popup_state_manager()->SetPopupState(
        OmniboxPopupState::kClassic);
  }

  TestOmniboxView* view() { return view_.get(); }

  OmniboxController* controller() { return omnibox_controller_.get(); }

  TestOmniboxEditModel* model() {
    return static_cast<TestOmniboxEditModel*>(
        omnibox_controller_->edit_model());
  }

  TestOmniboxClient* client() { return omnibox_client_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<OmniboxController> omnibox_controller_;
  std::unique_ptr<TestOmniboxView> view_;
  raw_ptr<TestOmniboxClient> omnibox_client_;
  TestOmniboxPopupView popup_view_;
};
}  // namespace

#if !BUILDFLAG(IS_ANDROID)
// Tests GetIcon returns the default search icon when the match is a search
// query.
TEST_F(OmniboxViewTest, DISABLED_GetIcon_Default) {
  ui::ImageModel expected_icon =
      ui::ImageModel::FromVectorIcon(vector_icons::kSearchChromeRefreshIcon,
                                     gfx::kPlaceholderColor, gfx::kFaviconSize);

  ui::ImageModel icon = view()->GetIcon(
      gfx::kFaviconSize, gfx::kPlaceholderColor, gfx::kPlaceholderColor,
      gfx::kPlaceholderColor, gfx::kPlaceholderColor, base::DoNothing(), false);

  EXPECT_EQ(expected_icon, icon);
}

// Tests GetIcon returns the bookmark icon when the match is bookmarked.
TEST_F(OmniboxViewTest, DISABLED_GetIcon_BookmarkIcon) {
  const GURL kUrl("https://bookmarks.com");

  AutocompleteMatch match;
  match.destination_url = kUrl;
  model()->SetCurrentMatchForTest(match);

  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), 0,
                           u"a bookmark", kUrl);

  ui::ImageModel expected_icon =
      ui::ImageModel::FromVectorIcon(omnibox::kBookmarkChromeRefreshIcon,
                                     gfx::kPlaceholderColor, gfx::kFaviconSize);

  ui::ImageModel icon = view()->GetIcon(
      gfx::kFaviconSize, gfx::kPlaceholderColor, gfx::kPlaceholderColor,
      gfx::kPlaceholderColor, gfx::kPlaceholderColor, base::DoNothing(), false);

  EXPECT_EQ(expected_icon, icon);
}

// Tests GetIcon returns the keyword search provider favicon when the match is a
// non-Google search query.
TEST_F(OmniboxViewTest, GetIcon_NonGoogleKeywordSearch) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(SK_ColorRED);
  gfx::Image expected_image =
      gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

  EXPECT_CALL(*client(), GetFaviconForKeywordSearchProvider(_, _))
      .WillOnce(Return(expected_image));

  TemplateURLData data;
  data.SetKeyword(u"foo");
  data.SetURL("https://foo.com");
  TemplateURL* turl = controller()->client()->GetTemplateURLService()->Add(
      std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(turl);

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
  match.keyword = u"foo";
  model()->SetCurrentMatchForTest(match);

  ui::ImageModel image = view()->GetIcon(
      gfx::kFaviconSize, gfx::kPlaceholderColor, gfx::kPlaceholderColor,
      gfx::kPlaceholderColor, gfx::kPlaceholderColor, base::DoNothing(), false);
  gfx::test::CheckColors(bitmap.getColor(0, 0),
                         image.GetImage().ToSkBitmap()->getColor(0, 0));
}

// Tests GetIcon returns the website's favicon when the match is a website.
TEST_F(OmniboxViewTest, GetIcon_Favicon) {
  const GURL kUrl("https://woahDude.com");

  GURL page_url;
  EXPECT_CALL(*client(), GetFaviconForPageUrl(_, _))
      .WillOnce(DoAll(SaveArg<0>(&page_url), Return(gfx::Image())));

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
  match.destination_url = kUrl;
  model()->SetCurrentMatchForTest(match);

  view()->GetIcon(gfx::kFaviconSize, gfx::kPlaceholderColor,
                  gfx::kPlaceholderColor, gfx::kPlaceholderColor,
                  gfx::kPlaceholderColor, base::DoNothing(), false);

  EXPECT_EQ(page_url, kUrl);
}

// Tests GetIcon returns the search aggregator's favicon by bitmap when the
// match is a non-Google search query with search aggregator keyword.
TEST_F(OmniboxViewPopupTest, GetIcon_SearchAggregatorKeywordSearch) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(SK_ColorRED);
  gfx::Image expected_image =
      gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

  EXPECT_CALL(*client(), GetFaviconForKeywordSearchProvider(_, _)).Times(0);

  TemplateURLData data;
  data.SetKeyword(u"foo");
  data.SetURL("https://foo.com");
  data.favicon_url = GURL("https://foo.com/icon.png");
  data.policy_origin = TemplateURLData::PolicyOrigin::kSearchAggregator;
  TemplateURL* turl = controller()->client()->GetTemplateURLService()->Add(
      std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(turl);

  // Sets the icon bitmap for search aggregator.
  model()->SetIconBitmap(GURL("https://foo.com/icon.png"), bitmap);

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
  match.keyword = u"foo";
  model()->SetCurrentMatchForTest(match);

  ui::ImageModel image = view()->GetIcon(
      gfx::kFaviconSize, gfx::kPlaceholderColor, gfx::kPlaceholderColor,
      gfx::kPlaceholderColor, gfx::kPlaceholderColor, base::DoNothing(), false);
  gfx::test::CheckColors(bitmap.getColor(0, 0),
                         image.GetImage().ToSkBitmap()->getColor(0, 0));
}

// Tests GetIcon returns the website's favicon when the match is a website.
TEST_F(OmniboxViewPopupTest, GetIcon_IconUrl) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  bitmap.eraseColor(SK_ColorRED);

  EXPECT_CALL(*client(), GetFaviconForPageUrl(_, _)).Times(0);

  // Creates a set of matches.
  ACMatches matches;
  AutocompleteMatch match(nullptr, 1000, false,
                          AutocompleteMatchType::NAVSUGGEST);
  match.icon_url = GURL("https://example.com/icon.png");
  matches.push_back(match);
  AutocompleteResult* result =
      &controller()->autocomplete_controller()->published_result_;
  result->AppendMatches(matches);
  model()->SetCurrentMatchForTest(match);

  // Sets the icon bitmap for search aggregator match.
  model()->SetIconBitmap(GURL("https://example.com/icon.png"), bitmap);

  ui::ImageModel image = view()->GetIcon(
      gfx::kFaviconSize, gfx::kPlaceholderColor, gfx::kPlaceholderColor,
      gfx::kPlaceholderColor, gfx::kPlaceholderColor, base::DoNothing(), false);
  gfx::test::CheckColors(bitmap.getColor(0, 0),
                         image.GetImage().ToSkBitmap()->getColor(0, 0));
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Tests GetStateChanges correctly determines if text was deleted.
TEST_F(OmniboxViewTest, GetStateChanges_DeletedText) {
  {
    // Continuing autocompletion
    auto state_before =
        TestOmniboxView::CreateState("google.com", 10, 3);  // goo[gle.com]
    auto state_after = TestOmniboxView::CreateState("goog", 4, 4);  // goog|
    auto state_changes =
        TestOmniboxView::GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Typing not the autocompletion
    auto state_before =
        TestOmniboxView::CreateState("google.com", 1, 10);  // g[oogle.com]
    auto state_after = TestOmniboxView::CreateState("gi", 2, 2);  // gi|
    auto state_changes =
        TestOmniboxView::GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Deleting autocompletion
    auto state_before =
        TestOmniboxView::CreateState("google.com", 1, 10);       // g[oogle.com]
    auto state_after = TestOmniboxView::CreateState("g", 1, 1);  // g|
    auto state_changes =
        TestOmniboxView::GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
  {
    // Inserting
    auto state_before =
        TestOmniboxView::CreateState("goole.com", 3, 3);  // goo|le.com
    auto state_after =
        TestOmniboxView::CreateState("google.com", 4, 4);  // goog|le.com
    auto state_changes =
        TestOmniboxView::GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Deleting
    auto state_before =
        TestOmniboxView::CreateState("googgle.com", 5, 5);  // googg|le.com
    auto state_after =
        TestOmniboxView::CreateState("google.com", 4, 4);  // goog|le.com
    auto state_changes =
        TestOmniboxView::GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
  {
    // Replacing
    auto state_before =
        TestOmniboxView::CreateState("goojle.com", 3, 4);  // goo[j]le.com
    auto state_after =
        TestOmniboxView::CreateState("google.com", 4, 4);  // goog|le.com
    auto state_changes =
        TestOmniboxView::GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
}
