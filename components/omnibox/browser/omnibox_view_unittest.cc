// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/omnibox_view.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/test_omnibox_edit_model.h"
#include "components/omnibox/browser/test_omnibox_view.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/paint_vector_icon.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

using base::ASCIIToUTF16;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

namespace {

class OmniboxViewTest : public testing::Test {
 public:
  OmniboxViewTest()
      : bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()) {
    auto omnibox_client = std::make_unique<TestOmniboxClient>();
    omnibox_client_ = omnibox_client.get();
    EXPECT_CALL(*client(), GetBookmarkModel())
        .WillRepeatedly(Return(bookmark_model_.get()));

    view_ = std::make_unique<TestOmniboxView>(std::move(omnibox_client));
    view_->controller()->SetEditModelForTesting(
        std::make_unique<TestOmniboxEditModel>(view_->controller(), view_.get(),
                                               /*pref_service=*/nullptr));
  }

  TestOmniboxView* view() { return view_.get(); }

  TestOmniboxEditModel* model() {
    return static_cast<TestOmniboxEditModel*>(view_->model());
  }

  TestOmniboxClient* client() { return omnibox_client_; }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<TestOmniboxClient, DanglingUntriaged> omnibox_client_;
  std::unique_ptr<TestOmniboxView> view_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
};

TEST_F(OmniboxViewTest, TestStripSchemasUnsafeForPaste) {
  constexpr const char* urls[] = {
      " \x01 ",                                       // Safe query.
      "http://www.google.com?q=javascript:alert(0)",  // Safe URL.
      "JavaScript",                                   // Safe query.
      "javaScript:",                                  // Unsafe JS URL.
      " javaScript: ",                                // Unsafe JS URL.
      "javAscript:Javascript:javascript",             // Unsafe JS URL.
      "javAscript:alert(1)",                          // Unsafe JS URL.
      "javAscript:javascript:alert(2)",               // Single strip unsafe.
      "jaVascript:\njavaScript:\x01 alert(3) \x01",   // Single strip unsafe.
      ("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x11\x12\x13\x14\x15\x16\x17"
       "\x18\x19 JavaScript:alert(4)"),  // Leading control chars unsafe.
      "\x01\x02javascript:\x03\x04JavaScript:alert(5)"  // Embedded control
                                                        // characters unsafe.
  };

  constexpr const char* expecteds[] = {
      " \x01 ",                                       // Safe query.
      "http://www.google.com?q=javascript:alert(0)",  // Safe URL.
      "JavaScript",                                   // Safe query.
      "",                                             // Unsafe JS URL.
      "",                                             // Unsafe JS URL.
      "javascript",                                   // Unsafe JS URL.
      "alert(1)",                                     // Unsafe JS URL.
      "alert(2)",                                     // Single strip unsafe.
      "alert(3) \x01",                                // Single strip unsafe.
      "alert(4)",  // Leading control chars unsafe.
      "alert(5)"   // Embedded control characters unsafe.
  };

  for (size_t i = 0; i < std::size(urls); i++) {
    EXPECT_EQ(ASCIIToUTF16(expecteds[i]),
              OmniboxView::StripJavascriptSchemas(base::UTF8ToUTF16(urls[i])));
  }
}

TEST_F(OmniboxViewTest, SanitizeTextForPaste) {
  const struct {
    std::u16string input;
    std::u16string output;
  } kTestcases[] = {
      // No whitespace: leave unchanged.
      {std::u16string(), std::u16string()},
      {u"a", u"a"},
      {u"abc", u"abc"},

      // Leading/trailing whitespace: remove.
      {u" abc", u"abc"},
      {u"  \n  abc", u"abc"},
      {u"abc ", u"abc"},
      {u"abc\t \t", u"abc"},
      {u"\nabc\n", u"abc"},

      // All whitespace: Convert to single space.
      {u" ", u" "},
      {u"\n", u" "},
      {u"   ", u" "},
      {u"\n\n\n", u" "},
      {u" \n\t", u" "},

      // Broken URL has newlines stripped.
      {u"http://www.chromium.org/developers/testing/chromium-\n"
       u"build-infrastructure/tour-of-the-chromium-buildbot",
       u"http://www.chromium.org/developers/testing/"
       u"chromium-build-infrastructure/tour-of-the-chromium-buildbot"},

      // Multi-line address is converted to a single-line address.
      {u"1600 Amphitheatre Parkway\nMountain View, CA",
       u"1600 Amphitheatre Parkway Mountain View, CA"},

      // Line-breaking the JavaScript scheme with no other whitespace results in
      // a
      // dangerous URL that is sanitized by dropping the scheme.
      {u"java\x0d\x0ascript:alert(0)", u"alert(0)"},

      // Line-breaking the JavaScript scheme with whitespace elsewhere in the
      // string results in a safe string with a space replacing the line break.
      {u"java\x0d\x0ascript: alert(0)", u"java script: alert(0)"},

      // Unusual URL with multiple internal spaces is preserved as-is.
      {u"http://foo.com/a.  b", u"http://foo.com/a.  b"},

      // URL with unicode whitespace is also preserved as-is.
      {u"http://foo.com/a\x3000"
       u"b",
       u"http://foo.com/a\x3000"
       u"b"},
  };

  for (const auto& testcase : kTestcases) {
    EXPECT_EQ(testcase.output,
              OmniboxView::SanitizeTextForPaste(testcase.input));
  }
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
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
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Tests GetStateChanges correctly determines if text was deleted.
TEST_F(OmniboxViewTest, GetStateChanges_DeletedText) {
  {
    // Continuing autocompletion
    auto state_before =
        TestOmniboxView::CreateState("google.com", 10, 3, 0);  // goo[gle.com]
    auto state_after = TestOmniboxView::CreateState("goog", 4, 4, 0);  // goog|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Typing not the autocompletion
    auto state_before =
        TestOmniboxView::CreateState("google.com", 1, 10, 0);  // g[oogle.com]
    auto state_after = TestOmniboxView::CreateState("gi", 2, 2, 0);  // gi|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Deleting autocompletion
    auto state_before =
        TestOmniboxView::CreateState("google.com", 1, 10, 0);  // g[oogle.com]
    auto state_after = TestOmniboxView::CreateState("g", 1, 1, 0);  // g|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
  {
    // Inserting
    auto state_before =
        TestOmniboxView::CreateState("goole.com", 3, 3, 0);  // goo|le.com
    auto state_after =
        TestOmniboxView::CreateState("google.com", 4, 4, 0);  // goog|le.com
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Deleting
    auto state_before =
        TestOmniboxView::CreateState("googgle.com", 5, 5, 0);  // googg|le.com
    auto state_after =
        TestOmniboxView::CreateState("google.com", 4, 4, 0);  // goog|le.com
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
  {
    // Replacing
    auto state_before =
        TestOmniboxView::CreateState("goojle.com", 3, 4, 0);  // goo[j]le.com
    auto state_after =
        TestOmniboxView::CreateState("google.com", 4, 4, 0);  // goog|le.com
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
}

// Tests GetStateChanges correctly determines if text was deleted.
TEST_F(OmniboxViewTest, GetStateChanges_DeletedText_RichAutocompletion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kRichAutocompletion,
      {{OmniboxFieldTrial::kRichAutocompletionAutocompleteNonPrefixAll.name,
        "true"}});

  // Cases with single selection

  {
    // Continuing autocompletion
    auto state_before =
        TestOmniboxView::CreateState("google.com", 10, 3, 7);  // goo[gle.com]
    auto state_after = TestOmniboxView::CreateState("goog", 4, 4, 0);  // goog|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Typing not the autocompletion
    auto state_before =
        TestOmniboxView::CreateState("google.com", 1, 10, 9);  // g[oogle.com]
    auto state_after = TestOmniboxView::CreateState("gi", 2, 2, 0);  // gi|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Deleting autocompletion
    auto state_before =
        TestOmniboxView::CreateState("google.com", 1, 10, 9);  // g[oogle.com]
    auto state_after = TestOmniboxView::CreateState("g", 1, 1, 0);  // g|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
  {
    // Inserting
    auto state_before =
        TestOmniboxView::CreateState("goole.com", 3, 3, 6);  // goo|le.com
    auto state_after =
        TestOmniboxView::CreateState("google.com", 4, 4, 0);  // goog|le.com
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Deleting
    auto state_before =
        TestOmniboxView::CreateState("googgle.com", 5, 5, 0);  // googg|le.com
    auto state_after =
        TestOmniboxView::CreateState("google.com", 4, 4, 0);  // goog|le.com
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
  {
    // Replacing
    auto state_before =
        TestOmniboxView::CreateState("goojle.com", 3, 4, 1);  // goo[j]le.com
    auto state_after =
        TestOmniboxView::CreateState("google.com", 4, 4, 0);  // goog|le.com
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }

  // Cases with multiselection

  {
    // Continuing autocompletion with multiselection
    auto state_before =
        TestOmniboxView::CreateState("google.com", 4, 10, 7);  // [g]oog[le.com]
    auto state_after = TestOmniboxView::CreateState("oogl", 4, 4, 0);  // oogl|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Typing not the autocompletion with multiselection
    auto state_before =
        TestOmniboxView::CreateState("google.com", 4, 10, 7);  // [g]oog[le.com]
    auto state_after = TestOmniboxView::CreateState("oogm", 4, 4, 0);  // oogm|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_FALSE(state_changes.just_deleted_text);
  }
  {
    // Deleting autocompletion with multiselection
    auto state_before =
        TestOmniboxView::CreateState("google.com", 4, 10, 7);  // [g]oog[le.com]
    auto state_after = TestOmniboxView::CreateState("oog", 3, 3, 0);  // oog|
    auto state_changes = view()->GetStateChanges(state_before, state_after);
    EXPECT_TRUE(state_changes.just_deleted_text);
  }
}

}  // namespace
