// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_edit_model.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/test_omnibox_edit_controller.h"
#include "components/omnibox/browser/test_omnibox_edit_model.h"
#include "components/omnibox/browser/test_omnibox_view.h"
#include "components/omnibox/browser/test_toolbar_model.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxEditModelTest : public testing::Test {
 public:
  void SetUp() override {
    controller_ = std::make_unique<TestOmniboxEditController>();
    view_ = std::make_unique<TestOmniboxView>(controller_.get());
    view_->SetModel(
        std::make_unique<TestOmniboxEditModel>(view_.get(), controller_.get()));
  }

  TestOmniboxView* view() { return view_.get(); }
  TestToolbarModel* toolbar_model() { return controller_->GetToolbarModel(); }
  TestOmniboxEditModel* model() {
    return static_cast<TestOmniboxEditModel*>(view_->model());
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
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
      {"a.de/", 0, "", false, "HTtp://a.de/", "HTtp://a.de/", true,
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
  };

  for (size_t i = 0; i < arraysize(input); ++i) {
    toolbar_model()->set_formatted_full_url(
        base::ASCIIToUTF16(input[i].url_for_editing));
    toolbar_model()->set_url_for_display(
        base::ASCIIToUTF16(input[i].url_for_display));
    model()->ResetDisplayTexts();

    model()->SetInputInProgress(input[i].is_match_selected_in_popup);
    model()->SetPopupIsOpen(input[i].is_match_selected_in_popup);
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::NAVSUGGEST;
    match.destination_url = GURL(input[i].match_destination_url);
    model()->SetCurrentMatchForTest(match);

    base::string16 result = base::ASCIIToUTF16(input[i].input);
    GURL url;
    bool write_url;
    model()->AdjustTextForCopy(input[i].sel_start, &result, &url, &write_url);
    EXPECT_EQ(base::ASCIIToUTF16(input[i].expected_output), result)
        << "@: " << i;
    EXPECT_EQ(input[i].write_url, write_url) << " @" << i;
    if (write_url)
      EXPECT_EQ(input[i].expected_url, url.spec()) << " @" << i;
  }
}

// Tests that AdjustTextForCopy behaves properly with Query in Omnibox enabled.
// For more general tests of copy adjustment, see the AdjustTextForCopy test.
TEST_F(OmniboxEditModelTest, AdjustTextForCopyQueryInOmnibox) {
  toolbar_model()->set_url(GURL("https://www.example.com/"));
  toolbar_model()->set_url_for_display(base::ASCIIToUTF16("example.com"));

  TestOmniboxClient* client =
      static_cast<TestOmniboxClient*>(model()->client());
  client->SetFakeSearchTermsForQueryInOmnibox(base::ASCIIToUTF16("foobar"));
  model()->ResetDisplayTexts();

  // Verify that we copy the query verbatim when nothing has been modified.
  {
    base::string16 result = base::ASCIIToUTF16("foobar");
    GURL url;
    bool write_url;
    model()->AdjustTextForCopy(0, &result, &url, &write_url);

    EXPECT_EQ(base::ASCIIToUTF16("foobar"), result);
    EXPECT_EQ(GURL(), url);
    EXPECT_FALSE(write_url);
  }

  // Verify we copy the query verbatim even if the user has refined the query.
  {
    base::string16 result = base::ASCIIToUTF16("something else");
    GURL url;
    bool write_url;
    model()->AdjustTextForCopy(0, &result, &url, &write_url);

    EXPECT_EQ(base::ASCIIToUTF16("something else"), result);
    EXPECT_EQ(GURL(), url);
    EXPECT_FALSE(write_url);
  }
}

TEST_F(OmniboxEditModelTest, InlineAutocompleteText) {
  // Test if the model updates the inline autocomplete text in the view.
  EXPECT_EQ(base::string16(), view()->inline_autocomplete_text());
  model()->SetUserText(base::ASCIIToUTF16("he"));
  model()->OnPopupDataChanged(base::ASCIIToUTF16("llo"), nullptr,
                              base::string16(), false);
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("llo"), view()->inline_autocomplete_text());

  base::string16 text_before = base::ASCIIToUTF16("he");
  base::string16 text_after = base::ASCIIToUTF16("hel");
  OmniboxView::StateChanges state_changes{
      &text_before, &text_after, 3, 3, false, true, false, false};
  model()->OnAfterPossibleChange(state_changes, true);
  EXPECT_EQ(base::string16(), view()->inline_autocomplete_text());
  model()->OnPopupDataChanged(base::ASCIIToUTF16("lo"), nullptr,
                              base::string16(), false);
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("lo"), view()->inline_autocomplete_text());

  model()->Revert();
  EXPECT_EQ(base::string16(), view()->GetText());
  EXPECT_EQ(base::string16(), view()->inline_autocomplete_text());

  model()->SetUserText(base::ASCIIToUTF16("he"));
  model()->OnPopupDataChanged(base::ASCIIToUTF16("llo"), nullptr,
                              base::string16(), false);
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view()->GetText());
  EXPECT_EQ(base::ASCIIToUTF16("llo"), view()->inline_autocomplete_text());

  model()->AcceptTemporaryTextAsUserText();
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view()->GetText());
  EXPECT_EQ(base::string16(), view()->inline_autocomplete_text());
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
  toolbar_model()->set_url(GURL("http://localhost/"));
  toolbar_model()->set_url_for_display(base::ASCIIToUTF16("localhost"));
  model()->ResetDisplayTexts();

  // Tests that we use the formatted full URL instead of the elided URL to
  // generate matches.
  {
    AutocompleteMatch match = model()->CurrentMatch(nullptr);
    EXPECT_EQ(AutocompleteMatchType::URL_WHAT_YOU_TYPED, match.type);
    EXPECT_TRUE(model()->CurrentTextIsURL());
  }

  // Tests that when there is a Query in Omnibox, generate matches from the
  // query, instead of the full formatted URL.
  TestOmniboxClient* client =
      static_cast<TestOmniboxClient*>(model()->client());
  client->SetFakeSearchTermsForQueryInOmnibox(base::ASCIIToUTF16("foobar"));
  model()->ResetDisplayTexts();

  {
    AutocompleteMatch match = model()->CurrentMatch(nullptr);
    EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, match.type);
    EXPECT_FALSE(model()->CurrentTextIsURL());
  }
}

TEST_F(OmniboxEditModelTest, DisplayText) {
  toolbar_model()->set_url(GURL("https://www.example.com/"));
  toolbar_model()->set_url_for_display(base::ASCIIToUTF16("example.com"));

  // Verify we show the display text when there is no Query in Omnibox match.
  model()->ResetDisplayTexts();
#if defined(OS_IOS)
  // iOS OmniboxEditModel always provides the full URL as the OmniboxView
  // permanent display text.
  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.com/"),
            model()->GetPermanentDisplayText());
#else
  EXPECT_EQ(base::ASCIIToUTF16("example.com"),
            model()->GetPermanentDisplayText());
#endif

  base::string16 search_terms;
  EXPECT_FALSE(model()->GetQueryInOmniboxSearchTerms(&search_terms));
  EXPECT_TRUE(search_terms.empty());

  EXPECT_TRUE(model()->CurrentTextIsURL());

  // Verify we can unelide and show the full URL properly.
  model()->Unelide(false /* exit_query_in_omnibox */);
  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.com/"), view()->GetText());
  EXPECT_TRUE(model()->user_input_in_progress());
  EXPECT_TRUE(view()->IsSelectAll());
  EXPECT_TRUE(model()->CurrentTextIsURL());
}

TEST_F(OmniboxEditModelTest, DisplayAndExitQueryInOmnibox) {
  toolbar_model()->set_url(GURL("https://www.example.com/"));
  toolbar_model()->set_url_for_display(base::ASCIIToUTF16("example.com"));

  // Verify the displayed text when there is a Query in Omnibox match.
  TestOmniboxClient* client =
      static_cast<TestOmniboxClient*>(model()->client());
  client->SetFakeSearchTermsForQueryInOmnibox(base::ASCIIToUTF16("foobar"));
  model()->ResetDisplayTexts();
  EXPECT_EQ(base::ASCIIToUTF16("foobar"), model()->GetPermanentDisplayText());

  base::string16 search_terms;
  EXPECT_TRUE(model()->GetQueryInOmniboxSearchTerms(&search_terms));
  EXPECT_FALSE(search_terms.empty());
  EXPECT_EQ(base::ASCIIToUTF16("foobar"), search_terms);
  EXPECT_FALSE(model()->CurrentTextIsURL());

  // Verify we can exit Query in Omnibox mode properly.
  model()->Unelide(true /* exit_query_in_omnibox */);
  EXPECT_EQ(base::ASCIIToUTF16("https://www.example.com/"), view()->GetText());
  EXPECT_TRUE(model()->user_input_in_progress());
  EXPECT_TRUE(view()->IsSelectAll());
  EXPECT_TRUE(model()->CurrentTextIsURL());
}

TEST_F(OmniboxEditModelTest, DisablePasteAndGoForLongTexts) {
  EXPECT_TRUE(model()->OmniboxEditModel::CanPasteAndGo(
      base::ASCIIToUTF16("short text")));

  base::string16 almost_long_text = base::ASCIIToUTF16(
      std::string(OmniboxEditModel::kMaxPasteAndGoTextLength, '.'));
  EXPECT_TRUE(model()->OmniboxEditModel::CanPasteAndGo(almost_long_text));

  base::string16 long_text = base::ASCIIToUTF16(
      std::string(OmniboxEditModel::kMaxPasteAndGoTextLength + 1, '.'));
  EXPECT_FALSE(model()->OmniboxEditModel::CanPasteAndGo(long_text));
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
                                               4);

  // Entering keyword search mode should preserve the full display text as the
  // user text, and select all.
  model()->EnterKeywordModeForDefaultSearchProvider(
      KeywordModeEntryMethod::KEYBOARD_SHORTCUT);
  EXPECT_EQ(base::UTF8ToUTF16("user text"), model()->GetUserTextForTesting());
  EXPECT_EQ(base::UTF8ToUTF16("user text"), view()->GetText());
  EXPECT_TRUE(view()->IsSelectAll());

  // Deleting the user text and exiting keyword mode should clear everything.
  view()->SetUserText(base::string16());
  model()->ClearKeyword();
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
  model()->OnPopupDataChanged(base::UTF8ToUTF16("match text"), &destination_url,
                              base::string16(), false);

  // Entering keyword search mode should preserve temporary text as the user
  // text, and select all.
  model()->EnterKeywordModeForDefaultSearchProvider(
      KeywordModeEntryMethod::KEYBOARD_SHORTCUT);
  EXPECT_EQ(base::UTF8ToUTF16("match text"), model()->GetUserTextForTesting());
  EXPECT_EQ(base::UTF8ToUTF16("match text"), view()->GetText());
  EXPECT_TRUE(view()->IsSelectAll());
}
