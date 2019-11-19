// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_url_provider.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/history_quick_provider.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_fixer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"

using base::ASCIIToUTF16;
using base::Time;
using base::TimeDelta;

namespace {

struct TestURLInfo {
  const char* url;
  const char* title;
  int visit_count;
  int typed_count;
  int age_in_days;
  bool hidden = false;
} test_db[] = {
    {"http://www.google.com/", "Google", 3, 3, 80},

    // High-quality pages should get a host synthesized as a lower-quality
    // match.
    {"http://slashdot.org/favorite_page.html", "Favorite page", 200, 100, 80},

    // Less popular pages should have hosts synthesized as higher-quality
    // matches.
    {"http://kerneltrap.org/not_very_popular.html", "Less popular", 4, 0, 80},

    // Unpopular pages should not appear in the results at all.
    {"http://freshmeat.net/unpopular.html", "Unpopular", 1, 0, 80},

    // If a host has a match, we should pick it up during host synthesis.
    {"http://news.google.com/?ned=us&topic=n", "Google News - U.S.", 2, 2, 80},
    {"http://news.google.com/", "Google News", 1, 1, 80},

    // Matches that are normally not inline-autocompletable should be
    // autocompleted if they are shorter substitutes for longer matches that
    // would have been inline autocompleted.
    {"http://synthesisatest.com/foo/", "Test A", 1, 1, 80},
    {"http://synthesisbtest.com/foo/", "Test B", 1, 1, 80},
    {"http://synthesisbtest.com/foo/bar.html", "Test B Bar", 2, 2, 80},

    // Suggested short URLs must be "good enough" and must match user input.
    {"http://foo.com/", "Dir", 5, 5, 80},
    {"http://foo.com/dir/", "Dir", 2, 2, 80},
    {"http://foo.com/dir/another/", "Dir", 5, 1, 80},
    {"http://foo.com/dir/another/again/", "Dir", 10, 0, 80},
    {"http://foo.com/dir/another/again/myfile.html", "File", 10, 2, 80},

    // We throw in a lot of extra URLs here to make sure we're testing the
    // history database's query, not just the autocomplete provider.
    {"http://startest.com/y/a", "A", 2, 2, 80},
    {"http://startest.com/y/b", "B", 5, 2, 80},
    {"http://startest.com/x/c", "C", 5, 2, 80},
    {"http://startest.com/x/d", "D", 5, 5, 80},
    {"http://startest.com/y/e", "E", 4, 2, 80},
    {"http://startest.com/y/f", "F", 3, 2, 80},
    {"http://startest.com/y/g", "G", 3, 2, 80},
    {"http://startest.com/y/h", "H", 3, 2, 80},
    {"http://startest.com/y/i", "I", 3, 2, 80},
    {"http://startest.com/y/j", "J", 3, 2, 80},
    {"http://startest.com/y/k", "K", 3, 2, 80},
    {"http://startest.com/y/l", "L", 3, 2, 80},
    {"http://startest.com/y/m", "M", 3, 2, 80},

    // A file: URL is useful for testing that fixup does the right thing w.r.t.
    // the number of trailing slashes on the user's input.
    {"file:///C:/foo.txt", "", 2, 2, 80},

    // Results with absurdly high typed_counts so that very generic queries like
    // "http" will give consistent results even if more data is added above.
    {"http://bogussite.com/a", "Bogus A", 10002, 10000, 80},
    {"http://bogussite.com/b", "Bogus B", 10001, 10000, 80},
    {"http://bogussite.com/c", "Bogus C", 10000, 10000, 80},

    // Domain name with number.
    {"http://www.17173.com/", "Domain with number", 3, 3, 80},

    // URLs to test exact-matching behavior.
    {"http://go/", "Intranet URL", 1, 1, 80},
    {"http://gooey/", "Intranet URL 2", 5, 5, 80},
    // This entry is explicitly added as hidden
    {"http://g/", "Intranet URL", 7, 7, 80, true},

    // URLs for testing offset adjustment.
    {"http://www.\xEA\xB5\x90\xEC\x9C\xA1.kr/", "Korean", 2, 2, 80},
    {"http://spaces.com/path%20with%20spaces/foo.html", "Spaces", 2, 2, 80},
    {"http://ms/c++%20style%20guide", "Style guide", 2, 2, 80},

    // URLs for testing ctrl-enter behavior.
    {"http://binky/", "Intranet binky", 2, 2, 80},
    {"http://winky/", "Intranet winky", 2, 2, 80},
    {"http://www.winky.com/", "Internet winky", 5, 0, 80},

    // URLs used by EmptyVisits.
    {"http://pandora.com/", "Pandora", 2, 2, 80},
    {"http://pa/", "pa", 0, 0, history::kLowQualityMatchAgeLimitInDays - 1},

    // For intranet based tests.
    {"http://intra/one", "Intranet", 2, 2, 80},
    {"http://intra/two", "Intranet two", 1, 1, 80},
    {"http://intra/three", "Intranet three", 2, 2, 80},
    {"https://www.prefixintra/one", "Intranet www", 1, 1, 80},
    {"http://moo/bar", "Intranet moo", 1, 1, 80},
    {"http://typedhost/typedpath", "Intranet typed", 1, 1, 80},
    {"http://typedhost/untypedpath", "Intranet untyped", 1, 0, 80},

    {"http://x.com/one", "Internet", 2, 2, 80},
    {"http://x.com/two", "Internet two", 1, 1, 80},
    {"http://x.com/three", "Internet three", 2, 2, 80},

    // For punycode tests.
    {"http://puny.xn--h2by8byc123p.in/", "Punycode", 2, 2, 5},
    {"http://two_puny.xn--1lq90ic7f1rc.cn/",
     "Punycode to be rendered in Unicode", 2, 2, 5},

    // For experimental HUP scoring test.
    {"http://7.com/1a", "One", 8, 4, 4},
    {"http://7.com/2a", "Two A", 4, 2, 8},
    {"http://7.com/2b", "Two B", 4, 1, 8},
    {"http://7.com/3a", "Three", 2, 1, 16},
    {"http://7.com/4a", "Four A", 1, 1, 32},
    {"http://7.com/4b", "Four B", 1, 1, 64},
    {"http://7.com/5a", "Five A", 8, 0, 64},  // never typed.

    // For match URL formatting test.
    {"https://www.abc.def.com/path", "URL with subdomain", 10, 10, 80},
    {"https://www.hij.com/path", "URL with www only", 10, 10, 80},

    // For URL-what-you-typed in history tests.
    {"https://wytih/", "What you typed in history main", 1, 1, 80},
    {"https://www.wytih/", "What you typed in history www main", 2, 2, 80},
    {"https://www.wytih/page", "What you typed in history www page", 5, 5, 80},
    {"ftp://wytih/file", "What you typed in history ftp file", 6, 6, 80},
    {"https://www.wytih/file", "What you typed in history www file", 7, 7, 80},

    // URLs containing whitespaces for inline autocompletion tests.
    {"https://www.zebra.com/zebra", "zebra1", 7, 7, 80},
    {"https://www.zebra.com/zebras", "zebra2", 7, 7, 80},
    {"https://www.zebra.com/zebra s", "zebra3", 7, 7, 80},
    {"https://www.zebra.com/zebra  s", "zebra4", 7, 7, 80},
};

}  // namespace

class HistoryURLProviderTest : public testing::Test,
                               public AutocompleteProviderListener {
 public:
  struct UrlAndLegalDefault {
    std::string url;
    bool allowed_to_be_default_match;
  };

  HistoryURLProviderTest()
      : sort_matches_(false) {
    HistoryQuickProvider::set_disabled(true);
  }

  ~HistoryURLProviderTest() override {
    HistoryQuickProvider::set_disabled(false);
  }

  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override;

 protected:
  // testing::Test
  void SetUp() override { ASSERT_TRUE(SetUpImpl(true)); }
  void TearDown() override;

  // Does the real setup.
  bool SetUpImpl(bool create_history_db) WARN_UNUSED_RESULT;

  // Fills test data into the history system.
  void FillData();

  // Runs an autocomplete query on |text| and checks to see that the returned
  // results' destination URLs match those provided.  Also allows checking
  // that the input type was identified correctly.
  void RunTest(const base::string16& text,
               const std::string& desired_tld,
               bool prevent_inline_autocomplete,
               const UrlAndLegalDefault* expected_urls,
               size_t num_results,
               metrics::OmniboxInputType* identified_input_type);

  // A version of the above without the final |type| output parameter.
  void RunTest(const base::string16& text,
               const std::string& desired_tld,
               bool prevent_inline_autocomplete,
               const UrlAndLegalDefault* expected_urls,
               size_t num_results) {
    metrics::OmniboxInputType type;
    return RunTest(text, desired_tld, prevent_inline_autocomplete,
                   expected_urls, num_results, &type);
  }

  // Verifies that for the given |input_text|, the first match's contents
  // are |expected_match_contents|. Also verifies that there is a correctly
  // positioned match classification within the contents.
  void ExpectFormattedFullMatch(const std::string& input_text,
                                const wchar_t* expected_match_contents,
                                size_t expected_match_location,
                                size_t expected_match_length);

  base::test::TaskEnvironment task_environment_;
  ACMatches matches_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<HistoryURLProvider> autocomplete_;
  // Should the matches be sorted and duplicates removed?
  bool sort_matches_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryURLProviderTest);
};

class HistoryURLProviderTestNoDB : public HistoryURLProviderTest {
 protected:
  void SetUp() override { ASSERT_TRUE(SetUpImpl(false)); }
};

class HistoryURLProviderTestNoSearchProvider : public HistoryURLProviderTest {
 protected:
  void SetUp() override {
    DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(true);
    HistoryURLProviderTest::SetUp();
  }

  void TearDown() override {
    HistoryURLProviderTest::TearDown();
    DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(false);
  }
};

void HistoryURLProviderTest::OnProviderUpdate(bool updated_matches) {
  if (autocomplete_->done())
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

bool HistoryURLProviderTest::SetUpImpl(bool create_history_db) {
  client_ = std::make_unique<FakeAutocompleteProviderClient>(create_history_db);
  if (!client_->GetHistoryService())
    return false;
  autocomplete_ = base::MakeRefCounted<HistoryURLProvider>(client_.get(), this);
  FillData();
  return true;
}

void HistoryURLProviderTest::TearDown() {
  autocomplete_ = nullptr;
  client_.reset();
  task_environment_.RunUntilIdle();
}

void HistoryURLProviderTest::FillData() {
  // Most visits are a long time ago (some tests require this since we do some
  // special logic for things visited very recently). Note that this time must
  // be more recent than the "expire history" threshold for the data to be kept
  // in the main database.
  //
  // TODO(brettw) It would be nice if we could test this behavior, in which
  // case the time would be specifed in the test_db structure.
  const Time now = Time::Now();

  for (size_t i = 0; i < base::size(test_db); ++i) {
    const TestURLInfo& cur = test_db[i];
    const GURL current_url(cur.url);
    client_->GetHistoryService()->AddPageWithDetails(
        current_url, base::UTF8ToUTF16(cur.title), cur.visit_count,
        cur.typed_count, now - TimeDelta::FromDays(cur.age_in_days), cur.hidden,
        history::SOURCE_BROWSED);
  }
}

void HistoryURLProviderTest::RunTest(
    const base::string16& text,
    const std::string& desired_tld,
    bool prevent_inline_autocomplete,
    const UrlAndLegalDefault* expected_urls,
    size_t num_results,
    metrics::OmniboxInputType* identified_input_type) {
  AutocompleteInput input(text, base::string16::npos, desired_tld,
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_prevent_inline_autocomplete(prevent_inline_autocomplete);
  *identified_input_type = input.type();
  autocomplete_->Start(input, false);
  if (!autocomplete_->done())
    base::RunLoop().Run();

  matches_ = autocomplete_->matches();
  if (sort_matches_) {
    TemplateURLService* service = client_->GetTemplateURLService();
    for (auto i = matches_.begin(); i != matches_.end(); ++i) {
      i->ComputeStrippedDestinationURL(input, service);
    }
    AutocompleteResult::DeduplicateMatches(input.current_page_classification(),
                                           &matches_);
    std::sort(matches_.begin(), matches_.end(),
              &AutocompleteMatch::MoreRelevant);
  }
  SCOPED_TRACE(ASCIIToUTF16("input = ") + text);
  ASSERT_EQ(num_results, matches_.size()) << "Input text: " << text
                                          << "\nTLD: \"" << desired_tld << "\"";
  for (size_t i = 0; i < num_results; ++i) {
    EXPECT_EQ(expected_urls[i].url, matches_[i].destination_url.spec());
    EXPECT_EQ(expected_urls[i].allowed_to_be_default_match,
              matches_[i].allowed_to_be_default_match);
  }
}

void HistoryURLProviderTest::ExpectFormattedFullMatch(
    const std::string& input_text,
    const wchar_t* expected_match_contents,
    size_t expected_match_location,
    size_t expected_match_length) {
  base::string16 expected_match_contents_string =
      base::WideToUTF16(expected_match_contents);
  ASSERT_FALSE(expected_match_contents_string.empty());

  SCOPED_TRACE("input = " + input_text);
  SCOPED_TRACE(ASCIIToUTF16("expected_match_contents = ") +
               expected_match_contents_string);

  AutocompleteInput input(ASCIIToUTF16(input_text),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  autocomplete_->Start(input, false);
  if (!autocomplete_->done())
    base::RunLoop().Run();

  // Test the variations of URL formatting on the first match.
  auto& match = autocomplete_->matches().front();
  EXPECT_EQ(expected_match_contents_string, match.contents);

  // Verify pre-match portion classification, if it should exist.
  auto classification_it = match.contents_class.begin();
  ASSERT_NE(classification_it, match.contents_class.end());
  if (expected_match_location > 0) {
    EXPECT_EQ(ACMatchClassification::URL, classification_it->style);
    EXPECT_EQ(0U, classification_it->offset);
    ++classification_it;
  }

  // Verify the match portion classification.
  ASSERT_NE(classification_it, match.contents_class.end());
  EXPECT_EQ(ACMatchClassification::URL | ACMatchClassification::MATCH,
            classification_it->style);
  EXPECT_EQ(expected_match_location, classification_it->offset);
  ++classification_it;

  // Verify post-match portion classification, if it should exist.
  size_t post_match_offset = expected_match_location + expected_match_length;
  if (post_match_offset < expected_match_contents_string.length()) {
    ASSERT_NE(classification_it, match.contents_class.end());
    EXPECT_EQ(ACMatchClassification::URL, classification_it->style);
    EXPECT_EQ(post_match_offset, classification_it->offset);
  }
}

TEST_F(HistoryURLProviderTest, PromoteShorterURLs) {
  // Test that hosts get synthesized below popular pages.
  const UrlAndLegalDefault expected_nonsynth[] = {
    { "http://slashdot.org/favorite_page.html", false },
    { "http://slashdot.org/", false }
  };
  RunTest(ASCIIToUTF16("slash"), std::string(), true, expected_nonsynth,
          base::size(expected_nonsynth));

  // Test that hosts get synthesized above less popular pages.
  const UrlAndLegalDefault expected_synth[] = {
    { "http://kerneltrap.org/", false },
    { "http://kerneltrap.org/not_very_popular.html", false }
  };
  RunTest(ASCIIToUTF16("kernel"), std::string(), true, expected_synth,
          base::size(expected_synth));

  // Test that unpopular pages are ignored completely.
  RunTest(ASCIIToUTF16("fresh"), std::string(), true, nullptr, 0);

  // Test that if we create or promote shorter suggestions that would not
  // normally be inline autocompletable, we make them inline autocompletable if
  // the original suggestion (that we replaced as "top") was inline
  // autocompletable.
  const UrlAndLegalDefault expected_synthesisa[] = {
    { "http://synthesisatest.com/", true },
    { "http://synthesisatest.com/foo/", true }
  };
  RunTest(ASCIIToUTF16("synthesisa"), std::string(), false, expected_synthesisa,
          base::size(expected_synthesisa));
  EXPECT_LT(matches_.front().relevance, 1200);
  const UrlAndLegalDefault expected_synthesisb[] = {
    { "http://synthesisbtest.com/foo/", true },
    { "http://synthesisbtest.com/foo/bar.html", true }
  };
  RunTest(ASCIIToUTF16("synthesisb"), std::string(), false, expected_synthesisb,
          base::size(expected_synthesisb));
  EXPECT_GE(matches_.front().relevance, 1410);

  // Test that if we have a synthesized host that matches a suggestion, they
  // get combined into one.
  const UrlAndLegalDefault expected_combine[] = {
    { "http://news.google.com/", false },
    { "http://news.google.com/?ned=us&topic=n", false },
  };
  ASSERT_NO_FATAL_FAILURE(RunTest(ASCIIToUTF16("news"), std::string(), true,
                                  expected_combine,
                                  base::size(expected_combine)));
  // The title should also have gotten set properly on the host for the
  // synthesized one, since it was also in the results.
  EXPECT_EQ(ASCIIToUTF16("Google News"), matches_.front().description);

  // Test that short URL matching works correctly as the user types more
  // (several tests):
  // The entry for foo.com is the best of all five foo.com* entries.
  const UrlAndLegalDefault short_1[] = {
    { "http://foo.com/", false },
    { "http://foo.com/dir/another/again/myfile.html", false },
    { "http://foo.com/dir/", false }
  };
  RunTest(ASCIIToUTF16("foo"), std::string(), true, short_1,
          base::size(short_1));

  // When the user types the whole host, make sure we don't get two results for
  // it.
  const UrlAndLegalDefault short_2[] = {
    { "http://foo.com/", true },
    { "http://foo.com/dir/another/again/myfile.html", false },
    { "http://foo.com/dir/", false },
    { "http://foo.com/dir/another/", false }
  };
  RunTest(ASCIIToUTF16("foo.com"), std::string(), true, short_2,
          base::size(short_2));
  RunTest(ASCIIToUTF16("foo.com/"), std::string(), true, short_2,
          base::size(short_2));

  // The filename is the second best of the foo.com* entries, but there is a
  // shorter URL that's "good enough".  The host doesn't match the user input
  // and so should not appear.
  const UrlAndLegalDefault short_3[] = {
    { "http://foo.com/dir/another/", false },
    { "http://foo.com/d", true },
    { "http://foo.com/dir/another/again/myfile.html", false },
    { "http://foo.com/dir/", false }
  };
  RunTest(ASCIIToUTF16("foo.com/d"), std::string(), true, short_3,
          base::size(short_3));
  // If prevent_inline_autocomplete is false, we won't bother creating the
  // URL-what-you-typed match because we have promoted inline autocompletions.
  const UrlAndLegalDefault short_3_allow_inline[] = {
    { "http://foo.com/dir/another/", true },
    { "http://foo.com/dir/another/again/myfile.html", true },
    { "http://foo.com/dir/", true }
  };
  RunTest(ASCIIToUTF16("foo.com/d"), std::string(), false, short_3_allow_inline,
          base::size(short_3_allow_inline));

  // We shouldn't promote shorter URLs than the best if they're not good
  // enough.
  const UrlAndLegalDefault short_4[] = {
    { "http://foo.com/dir/another/again/myfile.html", false },
    { "http://foo.com/dir/another/a", true },
    { "http://foo.com/dir/another/again/", false }
  };
  RunTest(ASCIIToUTF16("foo.com/dir/another/a"), std::string(), true, short_4,
          base::size(short_4));
  // If prevent_inline_autocomplete is false, we won't bother creating the
  // URL-what-you-typed match because we have promoted inline autocompletions.
  const UrlAndLegalDefault short_4_allow_inline[] = {
    { "http://foo.com/dir/another/again/myfile.html", true },
    { "http://foo.com/dir/another/again/", true }
  };
  RunTest(ASCIIToUTF16("foo.com/dir/another/a"), std::string(), false,
          short_4_allow_inline, base::size(short_4_allow_inline));

  // Exact matches should always be best no matter how much more another match
  // has been typed.
  const UrlAndLegalDefault short_5a[] = {
    { "http://gooey/", true },
    { "http://www.google.com/", true },
    { "http://go/", true }
  };
  const UrlAndLegalDefault short_5b[] = {
    { "http://go/", true },
    { "http://gooey/", true },
    { "http://www.google.com/", true }
  };
  // Note that there is an http://g/ URL that is marked as hidden.  It shouldn't
  // show up at all.  This test implicitly tests this fact too.
  RunTest(ASCIIToUTF16("g"), std::string(), false, short_5a,
          base::size(short_5a));
  RunTest(ASCIIToUTF16("go"), std::string(), false, short_5b,
          base::size(short_5b));
}

TEST_F(HistoryURLProviderTest, CullRedirects) {
  // URLs we will be using, plus the visit counts they will initially get
  // (the redirect set below will also increment the visit counts). We want
  // the results to be in A,B,C order. Note also that our visit counts are
  // all high enough so that domain synthesizing won't get triggered.
  struct TestCase {
    const char* url;
    int count;
  } test_cases[] = {
    {"http://redirects/A", 30},
    {"http://redirects/B", 20},
    {"http://redirects/C", 10}
  };
  for (size_t i = 0; i < base::size(test_cases); ++i) {
    client_->GetHistoryService()->AddPageWithDetails(
        GURL(test_cases[i].url), ASCIIToUTF16("Title"), test_cases[i].count,
        test_cases[i].count, Time::Now(), false, history::SOURCE_BROWSED);
  }

  // Create a B->C->A redirect chain, but set the visit counts such that they
  // will appear in A,B,C order in the results. The autocomplete query will
  // search for the most recent visit when looking for redirects, so this will
  // be found even though the previous visits had no redirects.
  history::RedirectList redirects_to_a;
  redirects_to_a.push_back(GURL(test_cases[1].url));
  redirects_to_a.push_back(GURL(test_cases[2].url));
  redirects_to_a.push_back(GURL(test_cases[0].url));
  client_->GetHistoryService()->AddPage(
      GURL(test_cases[0].url), Time::Now(), nullptr, 0, GURL(), redirects_to_a,
      ui::PAGE_TRANSITION_TYPED, history::SOURCE_BROWSED, true);

  // Because all the results are part of a redirect chain with other results,
  // all but the first one (A) should be culled. We should get the default
  // "what you typed" result, plus this one.
  const base::string16 typing(ASCIIToUTF16("http://redirects/"));
  const UrlAndLegalDefault expected_results[] = {
    { test_cases[0].url, false },
    { base::UTF16ToUTF8(typing), true }
  };
  RunTest(typing, std::string(), true, expected_results,
          base::size(expected_results));

  // If prevent_inline_autocomplete is false, we won't bother creating the
  // URL-what-you-typed match because we have promoted inline autocompletions.
  // The result set should instead consist of a single URL representing the
  // whole set of redirects.
  const UrlAndLegalDefault expected_results_allow_inlining[] = {
    { test_cases[0].url, true }
  };
  RunTest(typing, std::string(), false, expected_results_allow_inlining,
          base::size(expected_results_allow_inlining));
}

TEST_F(HistoryURLProviderTestNoSearchProvider, WhatYouTypedNoSearchProvider) {
  // When no search provider is available, make sure we provide what-you-typed
  // matches for text that could be a URL.

  const UrlAndLegalDefault results_1[] = {
    { "http://wytmatch/", true }
  };
  RunTest(ASCIIToUTF16("wytmatch"), std::string(), false, results_1,
          base::size(results_1));

  RunTest(ASCIIToUTF16("wytmatch foo bar"), std::string(), false, nullptr, 0);
  RunTest(ASCIIToUTF16("wytmatch+foo+bar"), std::string(), false, nullptr, 0);

  const UrlAndLegalDefault results_2[] = {
    { "http://wytmatch+foo+bar.com/", true }
  };
  RunTest(ASCIIToUTF16("wytmatch+foo+bar.com"), std::string(), false, results_2,
          base::size(results_2));
}

TEST_F(HistoryURLProviderTest, WhatYouTyped) {
  // Make sure we suggest a What You Typed match at the right times.
  RunTest(ASCIIToUTF16("wytmatch"), std::string(), false, nullptr, 0);
  RunTest(ASCIIToUTF16("wytmatch foo bar"), std::string(), false, nullptr, 0);
  RunTest(ASCIIToUTF16("wytmatch+foo+bar"), std::string(), false, nullptr, 0);
  RunTest(ASCIIToUTF16("wytmatch+foo+bar.com"), std::string(), false, nullptr,
          0);

  const UrlAndLegalDefault results_1[] = {
    { "http://www.wytmatch.com/", true }
  };
  RunTest(ASCIIToUTF16("wytmatch"), "com", false, results_1,
          base::size(results_1));

  const UrlAndLegalDefault results_2[] = {
    { "http://wytmatch%20foo%20bar/", false }
  };
  RunTest(ASCIIToUTF16("http://wytmatch foo bar"), std::string(), false,
          results_2, base::size(results_2));

  const UrlAndLegalDefault results_3[] = {
    { "https://wytmatch%20foo%20bar/", false }
  };
  RunTest(ASCIIToUTF16("https://wytmatch foo bar"), std::string(), false,
          results_3, base::size(results_3));

  const UrlAndLegalDefault results_4[] = {{"https://wytih/", true},
                                          {"https://www.wytih/file", true},
                                          {"ftp://wytih/file", true},
                                          {"https://www.wytih/page", true}};
  RunTest(ASCIIToUTF16("wytih"), std::string(), false, results_4,
          base::size(results_4));

  const UrlAndLegalDefault results_5[] = {{"https://www.wytih/", true},
                                          {"https://www.wytih/file", true},
                                          {"https://www.wytih/page", true}};
  RunTest(ASCIIToUTF16("www.wytih"), std::string(), false, results_5,
          base::size(results_5));

  const UrlAndLegalDefault results_6[] = {{"ftp://wytih/file", true},
                                          {"https://www.wytih/file", true}};
  RunTest(ASCIIToUTF16("wytih/file"), std::string(), false, results_6,
          base::size(results_6));
}

TEST_F(HistoryURLProviderTest, Fixup) {
  // Test for various past crashes we've had.
  RunTest(ASCIIToUTF16("\\"), std::string(), false, nullptr, 0);
  RunTest(ASCIIToUTF16("#"), std::string(), false, nullptr, 0);
  RunTest(ASCIIToUTF16("%20"), std::string(), false, nullptr, 0);
  const UrlAndLegalDefault fixup_crash[] = {
    { "http://%EF%BD%A5@s/", false }
  };
  RunTest(base::WideToUTF16(L"\uff65@s"), std::string(), false, fixup_crash,
          base::size(fixup_crash));
  RunTest(base::WideToUTF16(L"\u2015\u2015@ \uff7c"), std::string(), false,
          nullptr, 0);

  // Fixing up "file:" should result in an inline autocomplete offset of just
  // after "file:", not just after "file://".
  const base::string16 input_1(ASCIIToUTF16("file:"));
  const UrlAndLegalDefault fixup_1[] = {
    { "file:///C:/foo.txt", true }
  };
  ASSERT_NO_FATAL_FAILURE(
      RunTest(input_1, std::string(), false, fixup_1, base::size(fixup_1)));
  EXPECT_EQ(ASCIIToUTF16("///C:/foo.txt"),
            matches_.front().inline_autocompletion);

  // Fixing up "http:/" should result in an inline autocomplete offset of just
  // after "http:/", not just after "http:".
  const base::string16 input_2(ASCIIToUTF16("http:/"));
  const UrlAndLegalDefault fixup_2[] = {
    { "http://bogussite.com/a", true },
    { "http://bogussite.com/b", true },
    { "http://bogussite.com/c", true }
  };
  ASSERT_NO_FATAL_FAILURE(
      RunTest(input_2, std::string(), false, fixup_2, base::size(fixup_2)));
  EXPECT_EQ(ASCIIToUTF16("/bogussite.com/a"),
            matches_.front().inline_autocompletion);

  // Adding a TLD to a small number like "56" should result in "www.56.com"
  // rather than "0.0.0.56.com".
  const UrlAndLegalDefault fixup_3[] = {
    { "http://www.56.com/", true }
  };
  RunTest(ASCIIToUTF16("56"), "com", true, fixup_3, base::size(fixup_3));

  // An input looks like a IP address like "127.0.0.1" should result in
  // "http://127.0.0.1/".
  const UrlAndLegalDefault fixup_4[] = {
    { "http://127.0.0.1/", true }
  };
  RunTest(ASCIIToUTF16("127.0.0.1"), std::string(), false, fixup_4,
          base::size(fixup_4));

  // An number "17173" should result in "http://www.17173.com/" in db.
  const UrlAndLegalDefault fixup_5[] = {
    { "http://www.17173.com/", true }
  };
  RunTest(ASCIIToUTF16("17173"), std::string(), false, fixup_5,
          base::size(fixup_5));
}

// Make sure the results for the input 'p' don't change between the first and
// second passes.
TEST_F(HistoryURLProviderTest, EmptyVisits) {
  // Wait for history to create the in memory DB.
  history::BlockUntilHistoryProcessesPendingRequests(
      client_->GetHistoryService());

  AutocompleteInput input(ASCIIToUTF16("pa"), metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  autocomplete_->Start(input, false);
  // HistoryURLProvider shouldn't be done (waiting on async results).
  EXPECT_FALSE(autocomplete_->done());

  // We should get back an entry for pandora.
  matches_ = autocomplete_->matches();
  ASSERT_GT(matches_.size(), 0u);
  EXPECT_EQ(GURL("http://pandora.com/"), matches_[0].destination_url);
  int pandora_relevance = matches_[0].relevance;

  // Run the message loop. When |autocomplete_| finishes the loop is quit.
  base::RunLoop().Run();
  EXPECT_TRUE(autocomplete_->done());
  matches_ = autocomplete_->matches();
  ASSERT_GT(matches_.size(), 0u);
  EXPECT_EQ(GURL("http://pandora.com/"), matches_[0].destination_url);
  EXPECT_EQ(pandora_relevance, matches_[0].relevance);
}

TEST_F(HistoryURLProviderTestNoDB, NavigateWithoutDB) {
  // Ensure that we will still produce matches for navigation when there is no
  // database.
  UrlAndLegalDefault navigation_1[] = {
    { "http://test.com/", true }
  };
  RunTest(ASCIIToUTF16("test.com"), std::string(), false, navigation_1,
          base::size(navigation_1));

  UrlAndLegalDefault navigation_2[] = {
    { "http://slash/", false }
  };
  RunTest(ASCIIToUTF16("slash"), std::string(), false, navigation_2,
          base::size(navigation_2));

  RunTest(ASCIIToUTF16("this is a query"), std::string(), false, nullptr, 0);
}

TEST_F(HistoryURLProviderTest, AutocompleteOnTrailingWhitespace) {
  struct AutocompletionExpectation {
    std::string fill_into_edit;
    std::string inline_autocompletion;
    bool allowed_to_be_default_match;
  };

  auto TestAutocompletion =
      [this](std::string input_text, bool input_prevent_inline_autocomplete,
             const std::vector<AutocompletionExpectation>& expectations) {
        const std::string debug = base::StringPrintf(
            "input text [%s], prevent inline [%d]", input_text.c_str(),
            input_prevent_inline_autocomplete);

        AutocompleteInput input(ASCIIToUTF16(input_text),
                                metrics::OmniboxEventProto::OTHER,
                                TestSchemeClassifier());
        input.set_prevent_inline_autocomplete(
            input_prevent_inline_autocomplete);
        autocomplete_->Start(input, false);
        if (!autocomplete_->done())
          base::RunLoop().Run();

        matches_ = autocomplete_->matches();
        EXPECT_EQ(matches_.size(), expectations.size()) << debug;
        for (size_t i = 0; i < matches_.size(); ++i) {
          EXPECT_EQ(matches_[i].fill_into_edit,
                    ASCIIToUTF16(expectations[i].fill_into_edit))
              << debug;
          if (matches_[i].allowed_to_be_default_match) {
            EXPECT_EQ(matches_[i].inline_autocompletion,
                      ASCIIToUTF16(expectations[i].inline_autocompletion))
                << debug;
          }
          EXPECT_EQ(matches_[i].allowed_to_be_default_match,
                    expectations[i].allowed_to_be_default_match)
              << debug;
        }
      };

  TestAutocompletion("zebra.com/zebra", false,
                     {
                         {"zebra.com/zebra", "", true},
                         {"https://www.zebra.com/zebras", "s", true},
                         {"https://www.zebra.com/zebra s", " s", true},
                         {"https://www.zebra.com/zebra  s", "  s", true},
                     });

  TestAutocompletion("zebra.com/zebra ", false,
                     {
                         {"zebra.com/zebra", "", true},
                         {"https://www.zebra.com/zebras", "", false},
                         {"https://www.zebra.com/zebra s", "s", true},
                         {"https://www.zebra.com/zebra  s", " s", true},
                     });

  TestAutocompletion("zebra.com/zebra  ", false,
                     {
                         {"zebra.com/zebra", "", true},
                         {"https://www.zebra.com/zebras", "", false},
                         {"https://www.zebra.com/zebra s", "", false},
                         {"https://www.zebra.com/zebra  s", "s", true},
                     });

  TestAutocompletion("zebra.com/zebra", true,
                     {
                         {"zebra.com/zebra", "", true},
                         {"https://www.zebra.com/zebras", "", false},
                         {"https://www.zebra.com/zebra s", "", false},
                         {"https://www.zebra.com/zebra  s", "", false},
                     });

  TestAutocompletion("zebra.com/zebras", false,
                     {
                         {"zebra.com/zebras", "", true},
                     });

  TestAutocompletion("zebra.com/zebra s", false,
                     {
                         {"zebra.com/zebra s", "", true},
                     });
}

TEST_F(HistoryURLProviderTest, TreatEmailsAsSearches) {
  // Visiting foo.com should not make this string be treated as a navigation.
  // That means the result should not be allowed to be default, and it should
  // be scored around 1200 rather than 1400+.
  const UrlAndLegalDefault expected[] = {
    { "http://user@foo.com/", false }
  };
  ASSERT_NO_FATAL_FAILURE(RunTest(ASCIIToUTF16("user@foo.com"), std::string(),
                                  false, expected, base::size(expected)));
  EXPECT_LE(1200, matches_[0].relevance);
  EXPECT_LT(matches_[0].relevance, 1210);
}

TEST_F(HistoryURLProviderTest, IntranetURLsWithPaths) {
  struct TestCase {
    const char* input;
    int relevance;
    bool allowed_to_be_default_match;
  } test_cases[] = {
    { "fooey", 0, false },
    { "fooey/", 1200, true },  // 1200 for URL would still navigate by default.
    { "fooey/a", 1200, false },    // 1200 for UNKNOWN would not.
    { "fooey/a b", 1200, false },  // Also UNKNOWN.
    { "gooey", 1410, true },
    { "gooey/", 1410, true },
    { "gooey/a", 1400, true },
    { "gooey/a b", 1400, true },
  };
  for (size_t i = 0; i < base::size(test_cases); ++i) {
    SCOPED_TRACE(test_cases[i].input);
    if (test_cases[i].relevance == 0) {
      RunTest(ASCIIToUTF16(test_cases[i].input), std::string(), false, nullptr,
              0);
    } else {
      const UrlAndLegalDefault output[] = {
          {url_formatter::FixupURL(test_cases[i].input, std::string()).spec(),
           test_cases[i].allowed_to_be_default_match}};
      ASSERT_NO_FATAL_FAILURE(RunTest(ASCIIToUTF16(test_cases[i].input),
                                      std::string(), false, output,
                                      base::size(output)));
      // Actual relevance should be at least what test_cases expects and
      // and no more than 10 more.
      EXPECT_LE(test_cases[i].relevance, matches_[0].relevance);
      EXPECT_LT(matches_[0].relevance, test_cases[i].relevance + 10);
    }
  }
}

// Makes sure autocompletion happens for intranet sites that have been
// previoulsy visited.
TEST_F(HistoryURLProviderTest, IntranetURLCompletion) {
  sort_matches_ = true;

  const UrlAndLegalDefault expected1[] = {
    { "http://intra/three", true },
    { "http://intra/two", true }
  };
  ASSERT_NO_FATAL_FAILURE(RunTest(ASCIIToUTF16("intra/t"), std::string(), false,
                                  expected1, base::size(expected1)));
  EXPECT_LE(1410, matches_[0].relevance);
  EXPECT_LT(matches_[0].relevance, 1420);
  // It uses the default scoring.
  EXPECT_EQ(matches_[1].relevance, 1203);

  const UrlAndLegalDefault expected2[] = {
    { "http://moo/b", true },
    { "http://moo/bar", true }
  };
  ASSERT_NO_FATAL_FAILURE(RunTest(ASCIIToUTF16("moo/b"), std::string(), false,
                                  expected2, base::size(expected2)));
  // The url what you typed match should be around 1400, otherwise the
  // search what you typed match is going to be first.
  EXPECT_LE(1400, matches_[0].relevance);
  EXPECT_LT(matches_[0].relevance, 1410);

  const UrlAndLegalDefault expected3[] = {{"http://intra/three", true},
                                          {"http://intra/one", true},
                                          {"http://intra/two", true}};
  RunTest(ASCIIToUTF16("intra"), std::string(), false, expected3,
          base::size(expected3));

  const UrlAndLegalDefault expected4[] = {{"http://intra/three", true},
                                          {"http://intra/one", true},
                                          {"http://intra/two", true}};
  RunTest(ASCIIToUTF16("intra/"), std::string(), false, expected4,
          base::size(expected4));

  const UrlAndLegalDefault expected5[] = {
    { "http://intra/one", true }
  };
  ASSERT_NO_FATAL_FAILURE(RunTest(ASCIIToUTF16("intra/o"), std::string(), false,
                                  expected5, base::size(expected5)));
  EXPECT_LE(1410, matches_[0].relevance);
  EXPECT_LT(matches_[0].relevance, 1420);

  const UrlAndLegalDefault expected6[] = {
    { "http://intra/x", true }
  };
  ASSERT_NO_FATAL_FAILURE(RunTest(ASCIIToUTF16("intra/x"), std::string(), false,
                                  expected6, base::size(expected6)));
  EXPECT_LE(1400, matches_[0].relevance);
  EXPECT_LT(matches_[0].relevance, 1410);

  const UrlAndLegalDefault expected7[] = {
    { "http://typedhost/untypedpath", true }
  };
  ASSERT_NO_FATAL_FAILURE(RunTest(ASCIIToUTF16("typedhost/untypedpath"),
                                  std::string(), false, expected7,
                                  base::size(expected7)));
  EXPECT_LE(1400, matches_[0].relevance);
  EXPECT_LT(matches_[0].relevance, 1410);

  const UrlAndLegalDefault expected8[] = {{"https://www.prefixintra/x", true}};
  ASSERT_NO_FATAL_FAILURE(RunTest(ASCIIToUTF16("prefixintra/x"), std::string(),
                                  false, expected8, base::size(expected8)));
}

TEST_F(HistoryURLProviderTest, CrashDueToFixup) {
  // This test passes if we don't crash.  The results don't matter.
  const char* const test_cases[] = {
    "//c",
    "\\@st",
    "view-source:x",
  };
  for (size_t i = 0; i < base::size(test_cases); ++i) {
    AutocompleteInput input(ASCIIToUTF16(test_cases[i]),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    autocomplete_->Start(input, false);
    if (!autocomplete_->done())
      base::RunLoop().Run();
  }
}

TEST_F(HistoryURLProviderTest, DoesNotProvideMatchesOnFocus) {
  AutocompleteInput input(ASCIIToUTF16("foo"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_from_omnibox_focus(true);
  autocomplete_->Start(input, false);
  EXPECT_TRUE(autocomplete_->matches().empty());
}

TEST_F(HistoryURLProviderTest, DoesNotInlinePunycodeMatches) {
  // A URL that matches due to a match in the punycode URL is allowed to be the
  // default match if the URL doesn't get rendered as international characters.
  const UrlAndLegalDefault expected1_true[] = {
    { "http://puny.xn--h2by8byc123p.in/", true },
  };
  RunTest(ASCIIToUTF16("pun"), std::string(), false, expected1_true,
          base::size(expected1_true));
  RunTest(ASCIIToUTF16("puny."), std::string(), false, expected1_true,
          base::size(expected1_true));
  RunTest(ASCIIToUTF16("puny.x"), std::string(), false, expected1_true,
          base::size(expected1_true));
  RunTest(ASCIIToUTF16("puny.xn"), std::string(), false, expected1_true,
          base::size(expected1_true));
  RunTest(ASCIIToUTF16("puny.xn--"), std::string(), false, expected1_true,
          base::size(expected1_true));
  RunTest(ASCIIToUTF16("puny.xn--h2"), std::string(), false, expected1_true,
          base::size(expected1_true));
  RunTest(ASCIIToUTF16("puny.xn--h2by8byc123p"), std::string(), false,
          expected1_true, base::size(expected1_true));
  RunTest(ASCIIToUTF16("puny.xn--h2by8byc123p."), std::string(), false,
          expected1_true, base::size(expected1_true));

  // When the punycode part of the URL is rendered as international characters,
  // this match should not be allowed to be the default match if the inline
  // autocomplete text starts in the middle of the international characters.
  const UrlAndLegalDefault expected2_true[] = {
    { "http://two_puny.xn--1lq90ic7f1rc.cn/", true },
  };
  const UrlAndLegalDefault expected2_false[] = {
    { "http://two_puny.xn--1lq90ic7f1rc.cn/", false },
  };
  RunTest(ASCIIToUTF16("two"), std::string(), false, expected2_true,
          base::size(expected2_true));
  RunTest(ASCIIToUTF16("two_puny."), std::string(), false, expected2_true,
          base::size(expected2_true));
  RunTest(ASCIIToUTF16("two_puny.x"), std::string(), false, expected2_false,
          base::size(expected2_false));
  RunTest(ASCIIToUTF16("two_puny.xn"), std::string(), false, expected2_false,
          base::size(expected2_false));
  RunTest(ASCIIToUTF16("two_puny.xn--"), std::string(), false, expected2_false,
          base::size(expected2_false));
  RunTest(ASCIIToUTF16("two_puny.xn--1l"), std::string(), false,
          expected2_false, base::size(expected2_false));
  RunTest(ASCIIToUTF16("two_puny.xn--1lq90ic7f1rc"), std::string(), false,
          expected2_true, base::size(expected2_true));
  RunTest(ASCIIToUTF16("two_puny.xn--1lq90ic7f1rc."), std::string(), false,
          expected2_true, base::size(expected2_true));
}

TEST_F(HistoryURLProviderTest, CullSearchResults) {
  // Set up a default search engine.
  TemplateURLData data;
  data.SetShortName(ASCIIToUTF16("TestEngine"));
  data.SetKeyword(ASCIIToUTF16("TestEngine"));
  data.SetURL("http://testsearch.com/?q={searchTerms}");
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  template_url_service->Load();

  // URLs we will be using, plus the visit counts they will initially get
  // (the redirect set below will also increment the visit counts). We want
  // the results to be in A,B,C order. Note also that our visit counts are
  // all high enough so that domain synthesizing won't get triggered.
  struct TestCase {
    const char* url;
    int count;
  } test_cases[] = {
    {"https://testsearch.com/", 30},
    {"https://testsearch.com/?q=foobar", 20},
    {"http://foobar.com/", 10}
  };
  for (size_t i = 0; i < base::size(test_cases); ++i) {
    client_->GetHistoryService()->AddPageWithDetails(
        GURL(test_cases[i].url), base::UTF8ToUTF16("Title"),
        test_cases[i].count, test_cases[i].count, Time::Now(), false,
        history::SOURCE_BROWSED);
  }

  // We should not see search URLs when typing a previously used query.
  const UrlAndLegalDefault expected_when_searching_query[] = {
    { test_cases[2].url, false }
  };
  RunTest(ASCIIToUTF16("foobar"), std::string(), true,
          expected_when_searching_query,
          base::size(expected_when_searching_query));

  // We should not see search URLs when typing the search engine name.
  const UrlAndLegalDefault expected_when_searching_site[] = {
    { test_cases[0].url, false }
  };
  RunTest(ASCIIToUTF16("testsearch"), std::string(), true,
          expected_when_searching_site,
          base::size(expected_when_searching_site));
}

TEST_F(HistoryURLProviderTest, SuggestExactInput) {
  const size_t npos = std::string::npos;
  struct TestCase {
    // Inputs:
    const char* input;
    bool trim_http;
    // Expected Outputs:
    const char* contents;
    // Offsets of the ACMatchClassifications, terminated by npos.
    size_t offsets[3];
    // The index of the ACMatchClassification that should have the MATCH bit
    // set, npos if no ACMatchClassification should have the MATCH bit set.
    size_t match_classification_index;
  } test_cases[] = {
    { "http://www.somesite.com", false,
      "http://www.somesite.com", {0, npos, npos}, 0 },
    { "http://www.somesite.com/", false,
      "http://www.somesite.com", {0, npos, npos}, 0 },
    { "http://www.somesite.com/", false,
      "http://www.somesite.com", {0, npos, npos}, 0 },
    { "www.somesite.com", true,
      "www.somesite.com", {0, npos, npos}, 0 },
    { "somesite.com", true,
      "somesite.com", {0, npos, npos}, 0 },
    { "w", true,
      "w", {0, npos, npos}, 0 },
    { "w.com", true,
      "w.com", {0, npos, npos}, 0 },
    { "www.w.com", true,
      "www.w.com", {0, npos, npos}, 0 },
    { "view-source:w", true,
      "view-source:w", {0, npos, npos}, 0 },
    { "view-source:www.w.com/", true,
      "view-source:www.w.com", {0, npos, npos}, 0 },
    { "view-source:http://www.w.com/", false,
      "view-source:http://www.w.com", {0, npos, npos}, 0 },
    { "view-source:", true,
      "view-source:", {0, npos, npos}, 0 },
    { "http://w.com", false,
      "http://w.com", {0, npos, npos}, 0 },
    { "http://www.w.com", false,
      "http://www.w.com", {0, npos, npos}, 0 },
    { "http://a///www.w.com", false,
      "http://a///www.w.com", {0, npos, npos}, 0 },
    { "mailto://a@b.com", true,
      "mailto://a@b.com", {0, npos, npos}, 0 },
    { "mailto://a@b.com", false,
      "mailto://a@b.com", {0, npos, npos}, 0 },
    { "http://a%20b/x%20y", false,
      "http://a%20b/x y", {0, npos, npos}, 0 },
    { "file:///x%20y/a%20b", true,
      "file:///x y/a b", {0, npos, npos}, 0 },
    { "file://x%20y/a%20b", true,
      "file://x%20y/a b", {0, npos, npos}, 0 },
    { "view-source:x%20y/a%20b", true,
      "view-source:x%20y/a b", {0, npos, npos}, 0 },
    { "view-source:http://x%20y/a%20b", false,
      "view-source:http://x%20y/a b", {0, npos, npos}, 0 },
  };
  for (size_t i = 0; i < base::size(test_cases); ++i) {
    SCOPED_TRACE(testing::Message() << "Index " << i << " input: "
                                    << test_cases[i].input << ", trim_http: "
                                    << test_cases[i].trim_http);

    AutocompleteInput input(ASCIIToUTF16(test_cases[i].input),
                            metrics::OmniboxEventProto::BLANK,
                            TestSchemeClassifier());
    input.set_current_url(GURL("about:blank"));
    AutocompleteMatch match(autocomplete_->SuggestExactInput(
        input, input.canonicalized_url(), test_cases[i].trim_http));
    EXPECT_EQ(ASCIIToUTF16(test_cases[i].contents), match.contents);
    for (size_t match_index = 0; match_index < match.contents_class.size();
         ++match_index) {
      EXPECT_EQ(test_cases[i].offsets[match_index],
                match.contents_class[match_index].offset);
      EXPECT_EQ(ACMatchClassification::URL |
                (match_index == test_cases[i].match_classification_index ?
                 ACMatchClassification::MATCH : 0),
                match.contents_class[match_index].style);
    }
    EXPECT_EQ(npos, test_cases[i].offsets[match.contents_class.size()]);
  }
}

TEST_F(HistoryURLProviderTest, HUPScoringExperiment) {
  HUPScoringParams max_2000_no_time_decay;
  max_2000_no_time_decay.typed_count_buckets.buckets().push_back(
      std::make_pair(0.0, 2000));
  HUPScoringParams max_1250_no_time_decay;
  max_1250_no_time_decay.typed_count_buckets.buckets().push_back(
      std::make_pair(0.0, 1250));
  HUPScoringParams max_1000_no_time_decay;
  max_1000_no_time_decay.typed_count_buckets.buckets().push_back(
      std::make_pair(0.0, 1000));

  HUPScoringParams max_1100_with_time_decay_and_max_cap;
  max_1100_with_time_decay_and_max_cap.typed_count_buckets.
      set_relevance_cap(1400);
  max_1100_with_time_decay_and_max_cap.typed_count_buckets.
      set_half_life_days(16);
  max_1100_with_time_decay_and_max_cap.typed_count_buckets.buckets().push_back(
      std::make_pair(0.5, 1100));
  max_1100_with_time_decay_and_max_cap.typed_count_buckets.buckets().push_back(
      std::make_pair(0.24, 200));
  max_1100_with_time_decay_and_max_cap.typed_count_buckets.buckets().push_back(
      std::make_pair(0.0, 100));

  HUPScoringParams max_1100_visit_typed_decays;
  max_1100_visit_typed_decays.typed_count_buckets.set_half_life_days(16);
  max_1100_visit_typed_decays.typed_count_buckets.buckets().push_back(
      std::make_pair(0.5, 1100));
  max_1100_visit_typed_decays.typed_count_buckets.buckets().push_back(
      std::make_pair(0.0, 100));
  max_1100_visit_typed_decays.visited_count_buckets.set_half_life_days(16);
  max_1100_visit_typed_decays.visited_count_buckets.buckets().push_back(
      std::make_pair(0.5, 550));
  max_1100_visit_typed_decays.visited_count_buckets.buckets().push_back(
      std::make_pair(0.0, 50));

  const int kProviderMaxMatches = 3;
  struct TestCase {
    const char* input;
    HUPScoringParams scoring_params;
    struct ExpectedMatch {
      const char* url;
      int control_relevance;
      int experiment_relevance;
    };
    ExpectedMatch matches[kProviderMaxMatches];
  } test_cases[] = {
      // Max score 2000 -> no demotion.
      {"7.com/1",
       max_2000_no_time_decay,
       {{"7.com/1a", 1413, 1413}, {nullptr, 0, 0}, {nullptr, 0, 0}}},

      // Limit score to 1250/1000 and make sure that the top match is unchanged.
      {"7.com/1",
       max_1250_no_time_decay,
       {{"7.com/1a", 1413, 1413}, {nullptr, 0, 0}, {nullptr, 0, 0}}},
      {"7.com/2",
       max_1250_no_time_decay,
       {{"7.com/2a", 1413, 1413}, {"7.com/2b", 1412, 1250}, {nullptr, 0, 0}}},
      {"7.com/4",
       max_1000_no_time_decay,
       {{"7.com/4", 1203, 1203},
        {"7.com/4a", 1202, 1000},
        {"7.com/4b", 1201, 999}}},

      // Max relevance cap is 1400 and half-life is 16 days.
      {"7.com/1",
       max_1100_with_time_decay_and_max_cap,
       {{"7.com/1a", 1413, 1413}, {nullptr, 0, 0}, {nullptr, 0, 0}}},
      {"7.com/4",
       max_1100_with_time_decay_and_max_cap,
       {{"7.com/4", 1203, 1203},
        {"7.com/4a", 1202, 200},
        {"7.com/4b", 1201, 100}}},

      // Max relevance cap is 1400 and half-life is 16 days for both
      // visit/typed.
      {"7.com/5",
       max_1100_visit_typed_decays,
       {{"7.com/5", 1203, 1203}, {"7.com/5a", 1202, 50}, {nullptr, 0, 0}}},
  };
  for (size_t i = 0; i < base::size(test_cases); ++i) {
    SCOPED_TRACE(test_cases[i].input);
    UrlAndLegalDefault output[kProviderMaxMatches];
    int max_matches;
    for (max_matches = 0; max_matches < kProviderMaxMatches; ++max_matches) {
      if (test_cases[i].matches[max_matches].url == nullptr)
        break;
      output[max_matches].url =
          url_formatter::FixupURL(test_cases[i].matches[max_matches].url,
                                  std::string())
              .spec();
      output[max_matches].allowed_to_be_default_match = true;
    }
    autocomplete_->scoring_params_ = test_cases[i].scoring_params;

    // Test the experimental scoring params.
    ASSERT_NO_FATAL_FAILURE(RunTest(ASCIIToUTF16(test_cases[i].input),
                                    std::string(), false, output, max_matches));
    for (int j = 0; j < max_matches; ++j) {
      EXPECT_EQ(test_cases[i].matches[j].experiment_relevance,
                matches_[j].relevance);
    }
  }
}

TEST_F(HistoryURLProviderTest, MatchURLFormatting) {
  // Sanity check behavior under default flags.
  ExpectFormattedFullMatch("abc", L"www.abc.def.com/path", 4, 3);
  ExpectFormattedFullMatch("hij", L"hij.com/path", 0, 3);

  // Sanity check that scheme, subdomain, and path can all be trimmed or elided.
  ExpectFormattedFullMatch("hij", L"hij.com/path", 0, 3);

  // Verify that the scheme is preserved if part of match.
  ExpectFormattedFullMatch("https://www.hi", L"https://www.hij.com/path", 0,
                           14);

  // Verify that the whole subdomain is preserved if part of match.
  ExpectFormattedFullMatch("abc", L"www.abc.def.com/path", 4, 3);
  ExpectFormattedFullMatch("www.hij", L"www.hij.com/path", 0, 7);

  // Verify that the path is preserved if part of the match.
  ExpectFormattedFullMatch("hij.com/path", L"hij.com/path", 0, 12);

  // Verify preserving both the scheme and subdomain.
  ExpectFormattedFullMatch("https://www.hi", L"https://www.hij.com/path", 0,
                           14);

  // Verify preserving everything.
  ExpectFormattedFullMatch("https://www.hij.com/p", L"https://www.hij.com/path",
                           0, 21);

  // Verify that upper case input still works for subdomain matching.
  ExpectFormattedFullMatch("WWW.hij", L"www.hij.com/path", 0, 7);

  // Verify that matching in the subdomain-only preserves the subdomain.
  ExpectFormattedFullMatch("ww", L"www.hij.com/path", 0, 2);
  ExpectFormattedFullMatch("https://ww", L"https://www.hij.com/path", 0, 10);
}

std::unique_ptr<HistoryURLProviderParams> BuildHistoryURLProviderParams(
    const std::string& input_text,
    const std::string& url_text,
    bool match_in_scheme) {
  AutocompleteInput input(ASCIIToUTF16(input_text),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  history::HistoryMatch history_match;
  history_match.url_info.set_url(GURL(url_text));
  history_match.match_in_scheme = match_in_scheme;
  auto params = std::make_unique<HistoryURLProviderParams>(
      input, input, true, AutocompleteMatch(), nullptr, nullptr);
  params->matches.push_back(history_match);

  return params;
}

// Make sure "http://" scheme is generally trimmed.
TEST_F(HistoryURLProviderTest, DoTrimHttpScheme) {
  auto params =
      BuildHistoryURLProviderParams("face", "http://www.facebook.com", false);

  AutocompleteMatch match = autocomplete_->HistoryMatchToACMatch(*params, 0, 0);
  EXPECT_EQ(ASCIIToUTF16("facebook.com"), match.contents);
}

// Make sure "http://" scheme is not trimmed if input has a scheme too.
TEST_F(HistoryURLProviderTest, DontTrimHttpSchemeIfInputHasScheme) {
  auto params = BuildHistoryURLProviderParams("http://face",
                                              "http://www.facebook.com", false);

  AutocompleteMatch match = autocomplete_->HistoryMatchToACMatch(*params, 0, 0);
  EXPECT_EQ(ASCIIToUTF16("http://facebook.com"), match.contents);
}

// Make sure "http://" scheme is not trimmed if input matches in scheme.
TEST_F(HistoryURLProviderTest, DontTrimHttpSchemeIfInputMatchesInScheme) {
  auto params =
      BuildHistoryURLProviderParams("ht face", "http://www.facebook.com", true);

  AutocompleteMatch match = autocomplete_->HistoryMatchToACMatch(*params, 0, 0);
  EXPECT_EQ(ASCIIToUTF16("http://facebook.com"), match.contents);
}

// Make sure "https://" scheme is not trimmed if the input has a scheme.
TEST_F(HistoryURLProviderTest, DontTrimHttpsSchemeIfInputMatchesInScheme) {
  auto params = BuildHistoryURLProviderParams(
      "https://face", "https://www.facebook.com", false);

  AutocompleteMatch match = autocomplete_->HistoryMatchToACMatch(*params, 0, 0);
  EXPECT_EQ(ASCIIToUTF16("https://facebook.com"), match.contents);
}

// Make sure "https://" scheme is trimmed if nothing prevents it.
TEST_F(HistoryURLProviderTest, DoTrimHttpsScheme) {
  auto params =
      BuildHistoryURLProviderParams("face", "https://www.facebook.com", false);

  AutocompleteMatch match = autocomplete_->HistoryMatchToACMatch(*params, 0, 0);
  EXPECT_EQ(ASCIIToUTF16("facebook.com"), match.contents);
}
