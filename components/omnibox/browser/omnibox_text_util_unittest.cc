// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_text_util.h"

#include <stddef.h>

#include <array>
#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/url_formatter/url_fixer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxTextUtilTest : public testing::Test {
 public:
  OmniboxTextUtilTest() {
    omnibox_client_ = std::make_unique<TestOmniboxClient>();
  }

  void SetUp() override {
    omnibox::RegisterProfilePrefs(
        static_cast<sync_preferences::TestingPrefServiceSyncable*>(
            classifier_pref_service())
            ->registry());
  }

  PrefService* classifier_pref_service() {
    return client()
        ->autocomplete_classifier()
        ->autocomplete_controller()
        ->autocomplete_provider_client()
        ->GetPrefs();
  }
  TestOmniboxClient* client() { return omnibox_client_.get(); }
  TestLocationBarModel* location_bar_model() {
    return omnibox_client_->location_bar_model();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestOmniboxClient> omnibox_client_;
};

TEST_F(OmniboxTextUtilTest, TestStripSchemasUnsafeForPaste) {
  constexpr const auto urls = std::to_array<const char*>({
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
      "\x01\x02javascript:\x03\x04JavaScript:alert(5)",  // Embedded control
                                                         // characters unsafe.
  });

  constexpr const auto expecteds = std::to_array<const char*>({
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
      "alert(5)",  // Embedded control characters unsafe.
  });

  for (size_t i = 0; i < std::size(urls); i++) {
    EXPECT_EQ(base::ASCIIToUTF16(expecteds[i]),
              omnibox::StripJavascriptSchemas(base::UTF8ToUTF16(urls[i])));
  }
}

TEST_F(OmniboxTextUtilTest, SanitizeTextForPaste) {
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
    EXPECT_EQ(testcase.output, omnibox::SanitizeTextForPaste(testcase.input));
  }
}

// Tests various permutations of AutocompleteModel::AdjustTextForCopy.
TEST_F(OmniboxTextUtilTest, AdjustTextForCopy) {
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
  };
  auto input = std::to_array<Data>({
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
      {"https://ja.wikipedia.org/wiki/目次", 0, "", false,
       "https://ja.wikipedia.org/wiki/目次",
       "https://ja.wikipedia.org/wiki/%E7%9B%AE%E6%AC%A1", true,
       "https://ja.wikipedia.org/wiki/%E7%9B%AE%E6%AC%A1"},
      // Test escaping when part of the path was not copied.
      {"https://ja.wikipedia.org/wiki/目次", 0, "", false,
       "https://ja.wikipedia.org/wiki/目",
       "https://ja.wikipedia.org/wiki/%E7%9B%AE", true,
       "https://ja.wikipedia.org/wiki/%E7%9B%AE"},
      // Correctly handle escaping in the scheme-elided case as well.
      {"https://ja.wikipedia.org/wiki/目次", 0, "", false,
       "ja.wikipedia.org/wiki/目次",
       "https://ja.wikipedia.org/wiki/%E7%9B%AE%E6%AC%A1", true,
       "https://ja.wikipedia.org/wiki/%E7%9B%AE%E6%AC%A1",
       "ja.wikipedia.org/wiki/目次"},
      // Don't escape when host was modified.
      {"https://ja.wikipedia.org/wiki/目次", 0, "", false,
       "https://wikipedia.org/wiki/目次", "https://wikipedia.org/wiki/目次",
       false, ""},
  });

  for (size_t i = 0; i < std::size(input); ++i) {
    location_bar_model()->set_formatted_full_url(
        base::UTF8ToUTF16(input[i].url_for_editing));

    // Set the location bar model's URL to be a valid GURL that would generate
    // the test case's url_for_editing.
    location_bar_model()->set_url(
        url_formatter::FixupURL(input[i].url_for_editing, ""));

    bool is_popup_open = input[i].is_match_selected_in_popup;
    bool has_user_modified_text =
        is_popup_open || (input[i].input != input[i].url_for_editing &&
                          input[i].input != input[i].url_for_display);

    AutocompleteMatch match;
    match.type = AutocompleteMatchType::NAVSUGGEST;
    match.destination_url = GURL(input[i].match_destination_url);

    std::u16string result = base::UTF8ToUTF16(input[i].input);
    GURL url;
    bool write_url;
    omnibox::AdjustTextForCopy(
        input[i].sel_start, &result, has_user_modified_text,
        /*is_keyword_selected=*/false,
        is_popup_open ? std::optional<AutocompleteMatch>(match) : std::nullopt,
        client(), &url, &write_url);
    EXPECT_EQ(base::UTF8ToUTF16(input[i].expected_output), result)
        << "@: " << i;
    EXPECT_EQ(input[i].write_url, write_url) << " @" << i;
    if (write_url) {
      EXPECT_EQ(input[i].expected_url, url.spec()) << " @" << i;
    }
  }
}

// Tests that AdjustTextForCopy behaves properly for Reader Mode URLs.
TEST_F(OmniboxTextUtilTest, AdjustTextForCopyReaderMode) {
  const GURL article_url("https://www.example.com/article.html");
  const GURL distiller_url =
      dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
          dom_distiller::kDomDistillerScheme, article_url, "title");
  // In ReaderMode, the URL is chrome-distiller://<hash>,
  // but the user should only see the original URL minus the scheme.
  location_bar_model()->set_url(distiller_url);

  std::u16string result = base::UTF8ToUTF16(distiller_url.spec());
  GURL url;
  bool write_url = false;
  omnibox::AdjustTextForCopy(0, &result, false, false, std::nullopt, client(),
                             &url, &write_url);

  EXPECT_EQ(base::ASCIIToUTF16(article_url.spec()), result);
  EXPECT_EQ(article_url, url);
  EXPECT_TRUE(write_url);
}
