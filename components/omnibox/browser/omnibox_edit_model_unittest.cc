// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_edit_model.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/test_omnibox_edit_controller.h"
#include "components/omnibox/browser/test_omnibox_edit_model.h"
#include "components/omnibox/browser/test_omnibox_view.h"
#include "components/url_formatter/url_fixer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

using metrics::OmniboxEventProto;

class OmniboxEditModelTest : public testing::Test {
 public:
  void SetUp() override {
    controller_ = std::make_unique<TestOmniboxEditController>();
    view_ = std::make_unique<TestOmniboxView>(controller_.get());
    view_->SetModel(
        std::make_unique<TestOmniboxEditModel>(view_.get(), controller_.get()));
  }

  TestOmniboxView* view() { return view_.get(); }
  TestLocationBarModel* location_bar_model() {
    return controller_->GetLocationBarModel();
  }
  TestOmniboxEditModel* model() {
    return static_cast<TestOmniboxEditModel*>(view_->model());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestOmniboxEditController> controller_;
  std::unique_ptr<TestOmniboxView> view_;
};

// Tests various permutations of AutocompleteModel::AdjustTextForCopy.
TEST_F(OmniboxEditModelTest, AdjustTextForCopy) {
  struct Data {
    const char* url_for_editing;
    const int sel_start;

    const char* match_destination_url;
    const bool is_match_selected_in_popup;

    const char* input;
    const char* expected_output;
    const bool write_url;
    const char* expected_url;

    const char* url_for_display = "";
  } input[] = {
      // Test that http:// is inserted if all text is selected.
      {"a.de/b", 0, "", false, "a.de/b", "http://a.de/b", true,
       "http://a.de/b"},

      // Test that http:// and https:// are inserted if the host is selected.
      {"a.de/b", 0, "", false, "a.de/", "http://a.de/", true, "http://a.de/"},
      {"https://a.de/b", 0, "", false, "https://a.de/", "https://a.de/", true,
       "https://a.de/"},

      // Tests that http:// is inserted if the path is modified.
      {"a.de/b", 0, "", false, "a.de/c", "http://a.de/c", true,
       "http://a.de/c"},

      // Tests that http:// isn't inserted if the host is modified.
      {"a.de/b", 0, "", false, "a.com/b", "a.com/b", false, ""},

      // Tests that http:// isn't inserted if the start of the selection is 1.
      {"a.de/b", 1, "", false, "a.de/b", "a.de/b", false, ""},

      // Tests that http:// isn't inserted if a portion of the host is selected.
      {"a.de/", 0, "", false, "a.d", "a.d", false, ""},

      // Tests that http:// isn't inserted if the user adds to the host.
      {"a.de/", 0, "", false, "a.de.com/", "a.de.com/", false, ""},

      // Tests that we don't get double schemes if the user manually inserts
      // a scheme.
      {"a.de/", 0, "", false, "http://a.de/", "http://a.de/", true,
       "http://a.de/"},
      {"a.de/", 0, "", false, "HTtp://a.de/", "http://a.de/", true,
       "http://a.de/"},
      {"https://a.de/", 0, "", false, "https://a.de/", "https://a.de/", true,
       "https://a.de/"},

      // Test that we don't get double schemes or revert the change if the user
      // manually changes the scheme from 'http://' to 'https://' or vice versa.
      {"a.de/", 0, "", false, "https://a.de/", "https://a.de/", true,
       "https://a.de/"},
      {"https://a.de/", 0, "", false, "http://a.de/", "http://a.de/", true,
       "http://a.de/"},

      // Makes sure intranet urls get 'http://' prefixed to them.
      {"b/foo", 0, "", false, "b/foo", "http://b/foo", true, "http://b/foo",
       "b/foo"},

      // Verifies a search term 'foo' doesn't end up with http.
      {"www.google.com/search?", 0, "", false, "foo", "foo", false, ""},

      // Verifies that http:// and https:// are inserted for a match in a popup.
      {"a.com", 0, "http://b.com/foo", true, "b.com/foo", "http://b.com/foo",
       true, "http://b.com/foo"},
      {"a.com", 0, "https://b.com/foo", true, "b.com/foo", "https://b.com/foo",
       true, "https://b.com/foo"},

      // Even if the popup is open, if the input text doesn't correspond to the
      // current match, ignore the current match.
      {"a.com/foo", 0, "https://b.com/foo", true, "a.com/foo", "a.com/foo",
       false, "a.com/foo"},
      {"https://b.com/foo", 0, "https://b.com/foo", true, "https://b.co",
       "https://b.co", false, "https://b.co"},

      // Verifies that no scheme is inserted if there is no valid match.
      {"a.com", 0, "", true, "b.com/foo", "b.com/foo", false, ""},

      // Steady State Elisions test for re-adding an elided 'https://'.
      {"https://a.de/b", 0, "", false, "a.de/b", "https://a.de/b", true,
       "https://a.de/b", "a.de/b"},

      // Verifies that non-ASCII characters are %-escaped for valid copied URLs,
      // as long as the host has not been modified from the page URL.
      {u8"https://ja.wikipedia.org/wiki/目次", 0, "", false,
       u8"https://ja.wikipedia.org/wiki/目次",
       "https://ja.wikipedia.org/wiki/%E7%9B%AE%E6%AC%A1", true,
       "https://ja.wikipedia.org/wiki/%E7%9B%AE%E6%AC%A1"},
      // Test escaping when part of the path was not copied.
      {u8"https://ja.wikipedia.org/wiki/目次", 0, "", false,
       u8"https://ja.wikipedia.org/wiki/目",
       "https://ja.wikipedia.org/wiki/%E7%9B%AE", true,
       "https://ja.wikipedia.org/wiki/%E7%9B%AE"},
      // Correctly handle escaping in the scheme-elided case as well.
      {u8"https://ja.wikipedia.org/wiki/目次", 0, "", false,
       u8"ja.wikipedia.org/wiki/目次",
       "https://ja.wikipedia.org/wiki/%E7%9B%AE%E6%AC%A1", true,
       "https://ja.wikipedia.org/wiki/%E7%9B%AE%E6%AC%A1",
       u8"ja.wikipedia.org/wiki/目次"},
      // Don't escape when host was modified.
      {u8"https://ja.wikipedia.org/wiki/目次", 0, "", false,
       u8"https://wikipedia.org/wiki/目次", u8"https://wikipedia.org/wiki/目次",
       false, ""},
  };

  for (size_t i = 0; i < base::size(input); ++i) {
    location_bar_model()->set_formatted_full_url(
        base::UTF8ToUTF16(input[i].url_for_editing));
    location_bar_model()->set_url_for_display(
        base::UTF8ToUTF16(input[i].url_for_display));

    // Set the location bar model's URL to be a valid GURL that would generate
    // the test case's url_for_editing.
    location_bar_model()->set_url(
        url_formatter::FixupURL(input[i].url_for_editing, ""));

    model()->ResetDisplayTexts();

    model()->SetInputInProgress(input[i].is_match_selected_in_popup);
    model()->SetPopupIsOpen(input[i].is_match_selected_in_popup);
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::NAVSUGGEST;
    match.destination_url = GURL(input[i].match_destination_url);
    model()->SetCurrentMatchForTest(match);

    base::string16 result = base::UTF8ToUTF16(input[i].input);
    GURL url;
    bool write_url;
    model()->AdjustTextForCopy(input[i].sel_start, &result, &url, &write_url);
    EXPECT_EQ(base::UTF8ToUTF16(input[i].expected_output), result)
        << "@: " << i;
    EXPECT_EQ(input[i].write_url, write_url) << " @" << i;
    if (write_url)
      EXPECT_EQ(input[i].expected_url, url.spec()) << " @" << i;
  }
}

// Tests that AdjustTextForCopy behaves properly for Reader Mode URLs.
TEST_F(OmniboxEditModelTest, AdjustTextForCopyReaderMode) {
  const GURL article_url("https://www.example.com/article.html");
  const GURL distiller_url =
      dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
          dom_distiller::kDomDistillerScheme, article_url, "title");
  // In ReaderMode, the URL is chrome-distiller://<hash>,
  // but the user should only see the original URL minus the scheme.
  location_bar_model()->set_url(distiller_url);
  model()->ResetDisplayTexts();

  base::string16 result = base::UTF8ToUTF16(distiller_url.spec());
  GURL url;
  bool write_url = false;
  model()->AdjustTextForCopy(0, &result, &url, &write_url);

  EXPECT_EQ(base::ASCIIToUTF16(article_url.spec()), result);
  EXPECT_EQ(article_url, url);
  EXPECT_TRUE(write_url);
}

TEST_F(OmniboxEditModelTest, DISABLED_InlineAutocompleteText) {
  // Test if the model updates the inline autocomplete text in the view.
  EXPECT_EQ(base::string16(), view()->inline_autocompletion());
  model()->SetUserText(base::ASCIIToUTF16("he"));
  model()->OnPopupDataChanged(base::string16(),
                              /*is_temporary_text=*/false,
                              base::ASCIIToUTF16("llo"), base::string16(),
                              base::string16(), false, base::string16());
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("llo"), view()->inline_autocompletion());

  base::string16 text_before = base::ASCIIToUTF16("he");
  base::string16 text_after = base::ASCIIToUTF16("hel");
  OmniboxView::StateChanges state_changes{
      &text_before, &text_after, 3, 3, false, true, false, false};
  model()->OnAfterPossibleChange(state_changes, true);
  EXPECT_EQ(base::string16(), view()->inline_autocompletion());
  model()->OnPopupDataChanged(base::string16(),
                              /*is_temporary_text=*/false,
                              base::ASCIIToUTF16("lo"), base::string16(),
                              base::string16(), false, base::string16());
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("lo"), view()->inline_autocompletion());

  model()->Revert();
  EXPECT_EQ(base::string16(), view()->GetText());
  EXPECT_EQ(base::string16(), view()->inline_autocompletion());

  model()->SetUserText(base::ASCIIToUTF16("he"));
  model()->OnPopupDataChanged(base::string16(),
                              /*is_temporary_text=*/false,
                              base::ASCIIToUTF16("llo"), base::string16(),
                              base::string16(), false, base::string16());
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("llo"), view()->inline_autocompletion());

  model()->AcceptTemporaryTextAsUserText();
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view()->GetText());
  EXPECT_EQ(base::string16(), view()->inline_autocompletion());
}

// iOS doesn't use elisions in the Omnibox textfield.
#if !defined(OS_IOS)
TEST_F(OmniboxEditModelTest, RespectUnelisionInZeroSuggest) {
  location_bar_model()->set_url(GURL("https://www.example.com/"));
  location_bar_model()->set_url_for_display(base::ASCIIToUTF16("example.com"));

  EXPECT_TRUE(model()->ResetDisplayTexts());
  model()->Revert();

  // Set up view with unelided text.
  EXPECT_EQ(base::ASCIIToUTF16("example.com"), view()->GetText());
  EXPECT_TRUE(model()->Unelide());
  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.com/"), view()->GetText());
  EXPECT_FALSE(model()->user_input_in_progress());
  EXPECT_TRUE(view()->IsSelectAll());

  // Test that we don't clobber the unelided text with inline autocomplete text.
  EXPECT_EQ(base::string16(), view()->inline_autocompletion());
  model()->StartZeroSuggestRequest();
  model()->OnPopupDataChanged(base::string16(), /*is_temporary_text=*/false,
                              base::string16(), base::string16(),
                              base::string16(), false, base::string16());
  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.com/"), view()->GetText());
  EXPECT_FALSE(model()->user_input_in_progress());
  EXPECT_TRUE(view()->IsSelectAll());
}
#endif  // !defined(OS_IOS)

TEST_F(OmniboxEditModelTest, RevertZeroSuggestTemporaryText) {
  location_bar_model()->set_url(GURL("https://www.example.com/"));
  location_bar_model()->set_url_for_display(
      base::ASCIIToUTF16("https://www.example.com/"));

  EXPECT_TRUE(model()->ResetDisplayTexts());
  model()->Revert();

  // Simulate getting ZeroSuggestions and arrowing to a different match.
  view()->SelectAll(true);
  model()->StartZeroSuggestRequest();
  model()->OnPopupDataChanged(base::ASCIIToUTF16("fake_temporary_text"),
                              /*is_temporary_text=*/true, base::string16(),
                              base::string16(), base::string16(), false,
                              base::string16());

  // Test that reverting brings back the original input text.
  EXPECT_TRUE(model()->OnEscapeKeyPressed());
  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.com/"), view()->GetText());
  EXPECT_FALSE(model()->user_input_in_progress());
  EXPECT_TRUE(view()->IsSelectAll());
}

// This verifies the fix for a bug where calling OpenMatch() with a valid
// alternate nav URL would fail a DCHECK if the input began with "http://".
// The failure was due to erroneously trying to strip the scheme from the
// resulting fill_into_edit.  Alternate nav matches are never shown, so there's
// no need to ever try and strip this scheme.
TEST_F(OmniboxEditModelTest, AlternateNavHasHTTP) {
  const TestOmniboxClient* client =
      static_cast<TestOmniboxClient*>(model()->client());
  const AutocompleteMatch match(
      model()->autocomplete_controller()->search_provider(), 0, false,
      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  const GURL alternate_nav_url("http://abcd/");

  model()->OnSetFocus(false);  // Avoids DCHECK in OpenMatch().
  model()->SetUserText(base::ASCIIToUTF16("http://abcd"));
  model()->OpenMatch(match, WindowOpenDisposition::CURRENT_TAB,
                     alternate_nav_url, base::string16(), 0);
  EXPECT_TRUE(AutocompleteInput::HasHTTPScheme(
      client->alternate_nav_match().fill_into_edit));

  model()->SetUserText(base::ASCIIToUTF16("abcd"));
  model()->OpenMatch(match, WindowOpenDisposition::CURRENT_TAB,
                     alternate_nav_url, base::string16(), 0);
  EXPECT_TRUE(AutocompleteInput::HasHTTPScheme(
      client->alternate_nav_match().fill_into_edit));
}

TEST_F(OmniboxEditModelTest, CurrentMatch) {
  // Test the HTTP case.
  {
    location_bar_model()->set_url(GURL("http://www.example.com/"));
    location_bar_model()->set_url_for_display(
        base::ASCIIToUTF16("example.com"));
    model()->ResetDisplayTexts();
    model()->Revert();

    // iOS doesn't do elision in the textfield view.
#if defined(OS_IOS)
    EXPECT_EQ(base::ASCIIToUTF16("http://www.example.com/"), view()->GetText());
#else
    EXPECT_EQ(base::ASCIIToUTF16("example.com"), view()->GetText());
#endif

    AutocompleteMatch match = model()->CurrentMatch(nullptr);
    EXPECT_EQ(AutocompleteMatchType::URL_WHAT_YOU_TYPED, match.type);
    EXPECT_TRUE(model()->CurrentTextIsURL());
    EXPECT_EQ("http://www.example.com/", match.destination_url.spec());
  }

  // Test that generating a match from an elided HTTPS URL doesn't drop the
  // secure scheme.
  {
    location_bar_model()->set_url(GURL("https://www.google.com/"));
    location_bar_model()->set_url_for_display(base::ASCIIToUTF16("google.com"));
    model()->ResetDisplayTexts();
    model()->Revert();

    // iOS doesn't do elision in the textfield view.
#if defined(OS_IOS)
    EXPECT_EQ(base::ASCIIToUTF16("https://www.google.com/"), view()->GetText());
#else
    EXPECT_EQ(base::ASCIIToUTF16("google.com"), view()->GetText());
#endif

    AutocompleteMatch match = model()->CurrentMatch(nullptr);
    EXPECT_EQ(AutocompleteMatchType::URL_WHAT_YOU_TYPED, match.type);
    EXPECT_TRUE(model()->CurrentTextIsURL());

    // Additionally verify we aren't accidentally dropping the HTTPS scheme.
    EXPECT_EQ("https://www.google.com/", match.destination_url.spec());
  }
}

TEST_F(OmniboxEditModelTest, DisplayText) {
  location_bar_model()->set_url(GURL("https://www.example.com/"));
  location_bar_model()->set_url_for_display(base::ASCIIToUTF16("example.com"));

  EXPECT_TRUE(model()->ResetDisplayTexts());
  model()->Revert();

  EXPECT_TRUE(model()->CurrentTextIsURL());

#if defined(OS_IOS)
  // iOS OmniboxEditModel always provides the full URL as the OmniboxView
  // permanent display text. Unelision should return false.
  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.com/"),
            model()->GetPermanentDisplayText());
  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.com/"), view()->GetText());
  EXPECT_FALSE(model()->Unelide());
  EXPECT_FALSE(model()->user_input_in_progress());
  EXPECT_FALSE(view()->IsSelectAll());
#else
  // Verify we can unelide and show the full URL properly.
  EXPECT_EQ(base::ASCIIToUTF16("example.com"),
            model()->GetPermanentDisplayText());
  EXPECT_EQ(base::ASCIIToUTF16("example.com"), view()->GetText());
  EXPECT_TRUE(model()->Unelide());
  EXPECT_FALSE(model()->user_input_in_progress());
  EXPECT_TRUE(view()->IsSelectAll());
#endif

  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.com/"), view()->GetText());
  EXPECT_TRUE(model()->CurrentTextIsURL());

  // We should still show the current page's icon until the URL is modified.
  EXPECT_TRUE(model()->ShouldShowCurrentPageIcon());
  view()->SetUserText(base::ASCIIToUTF16("something else"));
  EXPECT_FALSE(model()->ShouldShowCurrentPageIcon());
}

TEST_F(OmniboxEditModelTest, UnelideDoesNothingWhenFullURLAlreadyShown) {
  location_bar_model()->set_url(GURL("https://www.example.com/"));
  location_bar_model()->set_url_for_display(
      base::ASCIIToUTF16("https://www.example.com/"));

  EXPECT_TRUE(model()->ResetDisplayTexts());
  model()->Revert();

  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.com/"),
            model()->GetPermanentDisplayText());
  EXPECT_TRUE(model()->CurrentTextIsURL());

  // Verify Unelide does nothing.
  EXPECT_FALSE(model()->Unelide());
  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.com/"), view()->GetText());
  EXPECT_FALSE(model()->user_input_in_progress());
  EXPECT_FALSE(view()->IsSelectAll());
  EXPECT_TRUE(model()->CurrentTextIsURL());
  EXPECT_TRUE(model()->ShouldShowCurrentPageIcon());
}

// The tab-switching system sometimes focuses the Omnibox even if it was not
// previously focused. In those cases, ignore the saved focus state.
TEST_F(OmniboxEditModelTest, IgnoreInvalidSavedFocusStates) {
  // The Omnibox starts out unfocused. Save that state.
  ASSERT_FALSE(model()->has_focus());
  OmniboxEditModel::State state = model()->GetStateForTabSwitch();
  ASSERT_EQ(OMNIBOX_FOCUS_NONE, state.focus_state);

  // Simulate the tab-switching system focusing the Omnibox.
  model()->OnSetFocus(false);

  // Restoring the old saved state should not clobber the model's focus state.
  model()->RestoreState(&state);
  EXPECT_TRUE(model()->has_focus());
  EXPECT_TRUE(model()->is_caret_visible());
}

// Tests ConsumeCtrlKey() consumes ctrl key when down, but does not affect ctrl
// state otherwise.
TEST_F(OmniboxEditModelTest, ConsumeCtrlKey) {
  model()->control_key_state_ = TestOmniboxEditModel::UP;
  model()->ConsumeCtrlKey();
  EXPECT_EQ(model()->control_key_state_, TestOmniboxEditModel::UP);
  model()->control_key_state_ = TestOmniboxEditModel::DOWN;
  model()->ConsumeCtrlKey();
  EXPECT_EQ(model()->control_key_state_,
            TestOmniboxEditModel::DOWN_AND_CONSUMED);
  model()->ConsumeCtrlKey();
  EXPECT_EQ(model()->control_key_state_,
            TestOmniboxEditModel::DOWN_AND_CONSUMED);
}

// Tests ctrl_key_state_ is set consumed if the ctrl key is down on focus.
TEST_F(OmniboxEditModelTest, ConsumeCtrlKeyOnRequestFocus) {
  model()->control_key_state_ = TestOmniboxEditModel::DOWN;
  model()->OnSetFocus(false);
  EXPECT_EQ(model()->control_key_state_, TestOmniboxEditModel::UP);
  model()->OnSetFocus(true);
  EXPECT_EQ(model()->control_key_state_,
            TestOmniboxEditModel::DOWN_AND_CONSUMED);
}

// Tests the ctrl key is consumed on a ctrl-action (e.g. ctrl-c to copy)
TEST_F(OmniboxEditModelTest, ConsumeCtrlKeyOnCtrlAction) {
  model()->control_key_state_ = TestOmniboxEditModel::DOWN;
  OmniboxView::StateChanges state_changes{nullptr, nullptr, 0,     0,
                                          false,   false,   false, false};
  model()->OnAfterPossibleChange(state_changes, false);
  EXPECT_EQ(model()->control_key_state_,
            TestOmniboxEditModel::DOWN_AND_CONSUMED);
}

TEST_F(OmniboxEditModelTest, KeywordModePreservesInlineAutocompleteText) {
  // Set the edit model into an inline autocompletion state.
  view()->SetUserText(base::UTF8ToUTF16("user"));
  view()->OnInlineAutocompleteTextMaybeChanged(base::UTF8ToUTF16("user text"),
                                               0, 4);

  // Entering keyword search mode should preserve the full display text as the
  // user text, and select all.
  model()->EnterKeywordModeForDefaultSearchProvider(
      OmniboxEventProto::KEYBOARD_SHORTCUT);
  EXPECT_EQ(base::UTF8ToUTF16("user text"), model()->GetUserTextForTesting());
  EXPECT_EQ(base::UTF8ToUTF16("user text"), view()->GetText());
  EXPECT_TRUE(view()->IsSelectAll());

  // Deleting the user text (exiting keyword) mode should clear everything.
  view()->SetUserText(base::string16());
  {
    EXPECT_TRUE(view()->GetText().empty());
    EXPECT_TRUE(model()->GetUserTextForTesting().empty());
    size_t start = 0, end = 0;
    view()->GetSelectionBounds(&start, &end);
    EXPECT_EQ(0U, start);
    EXPECT_EQ(0U, end);
  }
}

TEST_F(OmniboxEditModelTest, KeywordModePreservesTemporaryText) {
  // Set the edit model into a temporary text state.
  view()->SetUserText(base::UTF8ToUTF16("user text"));
  GURL destination_url("http://example.com");

  // OnPopupDataChanged() is called when the user focuses a suggestion.
  model()->OnPopupDataChanged(base::UTF8ToUTF16("match text"),
                              /*is_temporary_text=*/true, base::string16(),
                              base::string16(), base::string16(), false,
                              base::string16());

  // Entering keyword search mode should preserve temporary text as the user
  // text, and select all.
  model()->EnterKeywordModeForDefaultSearchProvider(
      OmniboxEventProto::KEYBOARD_SHORTCUT);
  EXPECT_EQ(base::UTF8ToUTF16("match text"), model()->GetUserTextForTesting());
  EXPECT_EQ(base::UTF8ToUTF16("match text"), view()->GetText());
  EXPECT_TRUE(view()->IsSelectAll());
}

TEST_F(OmniboxEditModelTest, CtrlEnterNavigatesToDesiredTLD) {
  // Set the edit model into an inline autocomplete state.
  view()->SetUserText(base::UTF8ToUTF16("foo"));
  model()->StartAutocomplete(false, false);
  view()->OnInlineAutocompleteTextMaybeChanged(base::UTF8ToUTF16("foobar"), 0,
                                               3);

  model()->OnControlKeyChanged(true);
  model()->AcceptInput(WindowOpenDisposition::UNKNOWN);
  OmniboxEditModel::State state = model()->GetStateForTabSwitch();
  EXPECT_EQ(GURL("http://www.foo.com/"),
            state.autocomplete_input.canonicalized_url());
}

TEST_F(OmniboxEditModelTest, CtrlEnterNavigatesToDesiredTLDTemporaryText) {
  // But if it's the temporary text, the View text should be used.
  view()->SetUserText(base::UTF8ToUTF16("foo"));
  model()->StartAutocomplete(false, false);
  model()->OnPopupDataChanged(base::ASCIIToUTF16("foobar"),
                              /*is_temporary_text=*/true, base::string16(),
                              base::string16(), base::string16(), false,
                              base::string16());

  model()->OnControlKeyChanged(true);
  model()->AcceptInput(WindowOpenDisposition::UNKNOWN);
  OmniboxEditModel::State state = model()->GetStateForTabSwitch();
  EXPECT_EQ(GURL("http://www.foobar.com/"),
            state.autocomplete_input.canonicalized_url());
}

TEST_F(OmniboxEditModelTest,
       CtrlEnterNavigatesToDesiredTLDSteadyStateElisions) {
  location_bar_model()->set_url(GURL("https://www.example.com/"));
  location_bar_model()->set_url_for_display(base::ASCIIToUTF16("example.com"));

  EXPECT_TRUE(model()->ResetDisplayTexts());
  model()->Revert();

  model()->OnControlKeyChanged(true);
  model()->AcceptInput(WindowOpenDisposition::UNKNOWN);
  OmniboxEditModel::State state = model()->GetStateForTabSwitch();
  EXPECT_EQ(GURL("https://www.example.com/"),
            state.autocomplete_input.canonicalized_url());
}
