// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/builtin_provider.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "url/gurl.h"

namespace {

const char kEmbedderAboutScheme[] = "chrome";
const char16_t kEmbedderAboutScheme16[] = u"chrome";
const char16_t kDefaultURL1[] = u"chrome://default1/";
const char16_t kDefaultURL2[] = u"chrome://default2/";
const char16_t kDefaultURL3[] = u"chrome://foo/";
const char16_t kSubpageURL[] = u"chrome://subpage/";

// Arbitrary host constants, chosen to start with the letters "b" and "me".
const char16_t kHostBar[] = u"bar";
const char16_t kHostMedia[] = u"media";
const char16_t kHostMemory[] = u"memory";
const char16_t kHostMemoryInternals[] = u"memory-internals";
const char16_t kHostSubpage[] = u"subpage";

const char16_t kSubpageOne[] = u"one";
const char16_t kSubpageTwo[] = u"two";
const char16_t kSubpageThree[] = u"three";

class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient() = default;
  FakeAutocompleteProviderClient(const FakeAutocompleteProviderClient&) =
      delete;
  FakeAutocompleteProviderClient& operator=(
      const FakeAutocompleteProviderClient&) = delete;

  std::string GetEmbedderRepresentationOfAboutScheme() const override {
    return kEmbedderAboutScheme;
  }

  std::vector<std::u16string> GetBuiltinURLs() override {
    std::vector<std::u16string> urls;
    urls.push_back(kHostBar);
    urls.push_back(kHostMedia);
    // The URL that is a superstring of the other is intentionally placed first
    // here. The provider makes no guarantees that shorter URLs will appear
    // first.
    urls.push_back(kHostMemoryInternals);
    urls.push_back(kHostMemory);
    urls.push_back(kHostSubpage);

    std::u16string prefix = base::StrCat({kHostSubpage, u"/"});
    urls.push_back(prefix + kSubpageOne);
    urls.push_back(prefix + kSubpageTwo);
    urls.push_back(prefix + kSubpageThree);
    return urls;
  }

  std::vector<std::u16string> GetBuiltinsToProvideAsUserTypes() override {
    std::vector<std::u16string> urls;
    urls.push_back(kDefaultURL1);
    urls.push_back(kDefaultURL2);
    urls.push_back(kDefaultURL3);
    return urls;
  }
};

}  // namespace

class BuiltinProviderTest : public testing::Test {
 public:
  BuiltinProviderTest(const BuiltinProviderTest&) = delete;
  BuiltinProviderTest& operator=(const BuiltinProviderTest&) = delete;

 protected:
  struct TestData {
    const std::u16string input;
    const std::vector<GURL> output;
  };

  BuiltinProviderTest() = default;
  ~BuiltinProviderTest() override = default;

  void SetUp() override {
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    provider_ = new BuiltinProvider(client_.get());
  }
  void TearDown() override { provider_ = nullptr; }

  void RunTest(const TestData cases[], size_t num_cases) {
    ACMatches matches;
    for (size_t i = 0; i < num_cases; ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "case %" PRIuS ": %s", i, base::UTF16ToUTF8(cases[i].input).c_str()));
      AutocompleteInput input(cases[i].input, metrics::OmniboxEventProto::OTHER,
                              TestSchemeClassifier());
      input.set_prevent_inline_autocomplete(true);
      provider_->Start(input, false);
      EXPECT_TRUE(provider_->done());
      matches = provider_->matches();
      ASSERT_EQ(cases[i].output.size(), matches.size());
      for (size_t j = 0; j < cases[i].output.size(); ++j) {
        EXPECT_EQ(cases[i].output[j], matches[j].destination_url);
      }
    }
  }

  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<BuiltinProvider> provider_;
};

TEST_F(BuiltinProviderTest, TypingScheme) {
  const std::u16string kAbout = url::kAboutScheme16;
  const std::u16string kEmbedder = kEmbedderAboutScheme16;
  const std::u16string kSeparator1 = u":";
  const std::u16string kSeparator2 = u":/";
  const std::u16string kSeparator3 = url::kStandardSchemeSeparator16;

  // These default URLs should correspond with those in BuiltinProvider::Start.
  const GURL kURL1(kDefaultURL1);
  const GURL kURL2(kDefaultURL2);
  const GURL kURL3(kDefaultURL3);

  TestData typing_scheme_cases[] = {
      // Typing an unrelated scheme should give nothing.
      {u"h", {}},
      {u"http", {}},
      {u"file", {}},
      {u"abouz", {}},
      {u"aboutt", {}},
      {u"aboutt:", {}},
      {u"chroma", {}},
      {u"chromee", {}},
      {u"chromee:", {}},

      // Typing a portion of about:// should give the default urls.
      {kAbout.substr(0, 1), {kURL1, kURL2, kURL3}},
      {u"A", {kURL1, kURL2, kURL3}},
      {kAbout, {kURL1, kURL2, kURL3}},
      {kAbout + kSeparator1, {kURL1, kURL2, kURL3}},
      {kAbout + kSeparator2, {kURL1, kURL2, kURL3}},
      {kAbout + kSeparator3, {kURL1, kURL2, kURL3}},
      {u"aBoUT://", {kURL1, kURL2, kURL3}},

      // Typing a portion of the embedder scheme should give the default urls.
      {kEmbedder.substr(0, 1), {kURL1, kURL2, kURL3}},
      {u"C", {kURL1, kURL2, kURL3}},
      {kEmbedder, {kURL1, kURL2, kURL3}},
      {kEmbedder + kSeparator1, {kURL1, kURL2, kURL3}},
      {kEmbedder + kSeparator2, {kURL1, kURL2, kURL3}},
      {kEmbedder + kSeparator3, {kURL1, kURL2, kURL3}},
      {u"ChRoMe://", {kURL1, kURL2, kURL3}},
  };

  RunTest(typing_scheme_cases, std::size(typing_scheme_cases));
}

TEST_F(BuiltinProviderTest, NonEmbedderURLs) {
  TestData test_cases[] = {
      // Typing an unrelated scheme should give nothing.
      {u"g@rb@g3", {}},
      {u"www.google.com", {}},
      {u"http:www.google.com", {}},
      {u"http://www.google.com", {}},
      {u"file:filename", {}},
      {u"scheme:", {}},
      {u"scheme://", {}},
      {u"scheme://host", {}},
      {u"scheme:host/path?query#ref", {}},
      {u"scheme://host/path?query#ref", {}},
  };

  RunTest(test_cases, std::size(test_cases));
}

TEST_F(BuiltinProviderTest, EmbedderProvidedURLs) {
  const std::u16string kAbout = url::kAboutScheme16;
  const std::u16string kEmbedder = kEmbedderAboutScheme16;
  const std::u16string kSep1 = u":";
  const std::u16string kSep2 = u":/";
  const std::u16string kSep3 = url::kStandardSchemeSeparator16;

  // The following hosts are arbitrary, chosen so that they all start with the
  // letters "me".
  const std::u16string kHostM1 = kHostMedia;
  const std::u16string kHostM2 = kHostMemoryInternals;
  const std::u16string kHostM3 = kHostMemory;
  const GURL kURLM1(kEmbedder + kSep3 + kHostM1);
  const GURL kURLM2(kEmbedder + kSep3 + kHostM2);
  const GURL kURLM3(kEmbedder + kSep3 + kHostM3);

  TestData test_cases[] = {
      // Typing an about URL with an unknown host should give nothing.
      {kAbout + kSep1 + u"host", {}},
      {kAbout + kSep2 + u"host", {}},
      {kAbout + kSep3 + u"host", {}},

      // Typing an embedder URL with an unknown host should give nothing.
      {kEmbedder + kSep1 + u"host", {}},
      {kEmbedder + kSep2 + u"host", {}},
      {kEmbedder + kSep3 + u"host", {}},

      // Typing an about URL should provide matching URLs.
      {kAbout + kSep1 + kHostM1.substr(0, 1), {kURLM1, kURLM2, kURLM3}},
      {kAbout + kSep2 + kHostM1.substr(0, 2), {kURLM1, kURLM2, kURLM3}},
      {kAbout + kSep3 + kHostM1.substr(0, 3), {kURLM1}},
      {kAbout + kSep3 + kHostM2.substr(0, 3), {kURLM2, kURLM3}},
      {kAbout + kSep3 + kHostM1, {kURLM1}},
      {kAbout + kSep2 + kHostM2, {kURLM2}},
      {kAbout + kSep2 + kHostM3, {kURLM2, kURLM3}},

      // Typing an embedder URL should provide matching URLs.
      {kEmbedder + kSep1 + kHostM1.substr(0, 1), {kURLM1, kURLM2, kURLM3}},
      {kEmbedder + kSep2 + kHostM1.substr(0, 2), {kURLM1, kURLM2, kURLM3}},
      {kEmbedder + kSep3 + kHostM1.substr(0, 3), {kURLM1}},
      {kEmbedder + kSep3 + kHostM2.substr(0, 3), {kURLM2, kURLM3}},
      {kEmbedder + kSep3 + kHostM1, {kURLM1}},
      {kEmbedder + kSep2 + kHostM2, {kURLM2}},
      {kEmbedder + kSep2 + kHostM3, {kURLM2, kURLM3}},
  };

  RunTest(test_cases, std::size(test_cases));
}

TEST_F(BuiltinProviderTest, AboutBlank) {
  const std::u16string kAbout = url::kAboutScheme16;
  const std::u16string kEmbedder = kEmbedderAboutScheme16;
  const std::u16string kAboutBlank = url::kAboutBlankURL16;
  const std::u16string kBlank = u"blank";
  const std::u16string kSeparator1 = url::kStandardSchemeSeparator16;
  const std::u16string kSeparator2 = u":///";
  const std::u16string kSeparator3 = u";///";

  const GURL kURLBar = GURL(kEmbedder + kSeparator1 + kHostBar);
  const GURL kURLBlank(kAboutBlank);

  TestData about_blank_cases[] = {
      // Typing an about:blank prefix should yield about:blank, among other
      // URLs.
      {kAboutBlank.substr(0, 7), {kURLBlank, kURLBar}},
      {kAboutBlank.substr(0, 8), {kURLBlank}},

      // Using any separator that is supported by fixup should yield
      // about:blank.
      // For now, BuiltinProvider does not suggest url-what-you-typed matches
      // for
      // for about:blank; check "about:blan" and "about;blan" substrings
      // instead.
      {kAbout + kSeparator2.substr(0, 1) + kBlank.substr(0, 4), {kURLBlank}},
      {kAbout + kSeparator2.substr(0, 2) + kBlank, {kURLBlank}},
      {kAbout + kSeparator2.substr(0, 3) + kBlank, {kURLBlank}},
      {kAbout + kSeparator2 + kBlank, {kURLBlank}},
      {kAbout + kSeparator3.substr(0, 1) + kBlank.substr(0, 4), {kURLBlank}},
      {kAbout + kSeparator3.substr(0, 2) + kBlank, {kURLBlank}},
      {kAbout + kSeparator3.substr(0, 3) + kBlank, {kURLBlank}},
      {kAbout + kSeparator3 + kBlank, {kURLBlank}},

      // Using the embedder scheme should not yield about:blank.
      {kEmbedder + kSeparator1.substr(0, 1) + kBlank, {}},
      {kEmbedder + kSeparator1.substr(0, 2) + kBlank, {}},
      {kEmbedder + kSeparator1.substr(0, 3) + kBlank, {}},
      {kEmbedder + kSeparator1 + kBlank, {}},

      // Adding trailing text should not yield about:blank.
      {kAboutBlank + u"/", {}},
      {kAboutBlank + u"/p", {}},
      {kAboutBlank + u"x", {}},
      {kAboutBlank + u"?q", {}},
      {kAboutBlank + u"#r", {}},

      // Interrupting "blank" with conflicting text should not yield
      // about:blank.
      {kAboutBlank.substr(0, 9) + u"/", {}},
      {kAboutBlank.substr(0, 9) + u"/p", {}},
      {kAboutBlank.substr(0, 9) + u"x", {}},
      {kAboutBlank.substr(0, 9) + u"?q", {}},
      {kAboutBlank.substr(0, 9) + u"#r", {}},
  };

  RunTest(about_blank_cases, std::size(about_blank_cases));
}

TEST_F(BuiltinProviderTest, DoesNotSupportMatchesOnFocus) {
  AutocompleteInput input(u"chrome://m", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  provider_->Start(input, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(BuiltinProviderTest, Subpages) {
  const std::u16string kSubpage = kSubpageURL;
  const std::u16string kPageOne = kSubpageOne;
  const std::u16string kPageTwo = kSubpageTwo;
  const std::u16string kPageThree = kSubpageThree;
  const GURL kURLOne(kSubpage + kPageOne);
  const GURL kURLTwo(kSubpage + kPageTwo);
  const GURL kURLThree(kSubpage + kPageThree);

  TestData settings_subpage_cases[] = {
      // Typing the settings path should show settings and the first two
      // subpages.
      {kSubpage, {GURL(kSubpage), kURLOne, kURLTwo}},

      // Typing a subpage path should return the appropriate results.
      {kSubpage + kPageTwo.substr(0, 1), {kURLTwo, kURLThree}},
      {kSubpage + kPageTwo.substr(0, 2), {kURLTwo}},
      {kSubpage + kPageThree.substr(0, kPageThree.length() - 1), {kURLThree}},
      {kSubpage + kPageOne, {kURLOne}},
      {kSubpage + kPageTwo, {kURLTwo}},
  };

  RunTest(settings_subpage_cases, std::size(settings_subpage_cases));
}

TEST_F(BuiltinProviderTest, Inlining) {
  const std::u16string kAbout = url::kAboutScheme16;
  const std::u16string kEmbedder = kEmbedderAboutScheme16;
  const std::u16string kSep = url::kStandardSchemeSeparator16;
  const std::u16string kHostM = kHostMedia;
  const std::u16string kHostB = kHostBar;
  const std::u16string kHostMem = kHostMemory;
  const std::u16string kHostMemInt = kHostMemoryInternals;
  const std::u16string kHostSub = kHostSubpage;
  const std::u16string kHostSubTwo =
      base::StrCat({kHostSubpage, u"/", kSubpageTwo});

  struct InliningTestData {
    const std::u16string input;
    const std::u16string expected_inline_autocompletion;
  } cases[] = {
      // Typing along "about://media" should not yield an inline autocompletion
      // until the completion is unique.  We don't bother checking every single
      // character before the first "m" is typed.
      {kAbout.substr(0, 2), std::u16string()},
      {kAbout, std::u16string()},
      {kAbout + kSep, std::u16string()},
      {kAbout + kSep + kHostM.substr(0, 1), std::u16string()},
      {kAbout + kSep + kHostM.substr(0, 2), std::u16string()},
      {kAbout + kSep + kHostM.substr(0, 3), kHostM.substr(3)},
      {kAbout + kSep + kHostM.substr(0, 4), kHostM.substr(4)},

      // Ditto with "chrome://media".
      {kEmbedder.substr(0, 2), std::u16string()},
      {kEmbedder, std::u16string()},
      {kEmbedder + kSep, std::u16string()},
      {kEmbedder + kSep + kHostM.substr(0, 1), std::u16string()},
      {kEmbedder + kSep + kHostM.substr(0, 2), std::u16string()},
      {kEmbedder + kSep + kHostM.substr(0, 3), kHostM.substr(3)},
      {kEmbedder + kSep + kHostM.substr(0, 4), kHostM.substr(4)},

      // The same rules should apply to "about://bar" and "chrome://bar".
      // At the "a" from "bar" in "about://bar", Chrome should be willing to
      // start inlining.  (Before that it conflicts with about:blank.)  At
      // the "b" from "bar" in "chrome://bar", Chrome should be willing to
      // start inlining.  (There is no chrome://blank page.)
      {kAbout + kSep + kHostB.substr(0, 1), std::u16string()},
      {kAbout + kSep + kHostB.substr(0, 2), kHostB.substr(2)},
      {kAbout + kSep + kHostB.substr(0, 3), kHostB.substr(3)},
      {kEmbedder + kSep + kHostB.substr(0, 1), kHostB.substr(1)},
      {kEmbedder + kSep + kHostB.substr(0, 2), kHostB.substr(2)},
      {kEmbedder + kSep + kHostB.substr(0, 3), kHostB.substr(3)},

      // The same rules should apply to "about://memory" and "chrome://memory".
      // At the second "m", an inline autocompletion should be offered. Although
      // this could also be completed with "memory-internals", "memory" is
      // shorter
      // and prefix of the other candidate, so it is preferred.
      {kAbout + kSep + kHostMem.substr(0, 1), std::u16string()},
      {kAbout + kSep + kHostMem.substr(0, 2), std::u16string()},
      {kAbout + kSep + kHostMem.substr(0, 3), kHostMem.substr(3)},
      {kAbout + kSep + kHostMem.substr(0, 4), kHostMem.substr(4)},
      {kEmbedder + kSep + kHostMem.substr(0, 1), std::u16string()},
      {kEmbedder + kSep + kHostMem.substr(0, 2), std::u16string()},
      {kEmbedder + kSep + kHostMem.substr(0, 3), kHostMem.substr(3)},
      {kEmbedder + kSep + kHostMem.substr(0, 4), kHostMem.substr(4)},

      // After "memory-", then "memory-internals" should be inlined.
      {kAbout + kSep + kHostMemInt.substr(0, 7), kHostMemInt.substr(7)},
      {kEmbedder + kSep + kHostMemInt.substr(0, 7), kHostMemInt.substr(7)},

      // Similarly, inline "about://subpage" and "chrome://subpage" even though
      // other, longer completions (e.g. "chrome://subpage/one") are available.
      {kAbout + kSep + kHostSub.substr(0, 1), kHostSub.substr(1)},
      {kAbout + kSep + kHostSub.substr(0, 2), kHostSub.substr(2)},
      {kAbout + kSep + kHostSub.substr(0, 3), kHostSub.substr(3)},
      {kEmbedder + kSep + kHostSub.substr(0, 1), kHostSub.substr(1)},
      {kEmbedder + kSep + kHostSub.substr(0, 2), kHostSub.substr(2)},
      {kEmbedder + kSep + kHostSub.substr(0, 3), kHostSub.substr(3)},

      // Once the user input distinctly matches a longer subpage
      // ("chrome://subpage/two"), inline that. This doesn't happen until the
      // user
      // enters "w" so that it it can be distinguished from
      // "chrome://subpage/three".
      {kAbout + kSep + kHostSubTwo.substr(0, 8), std::u16string()},
      {kAbout + kSep + kHostSubTwo.substr(0, 9), std::u16string()},
      {kAbout + kSep + kHostSubTwo.substr(0, 10), kHostSubTwo.substr(10)},
      {kEmbedder + kSep + kHostSubTwo.substr(0, 8), std::u16string()},
      {kEmbedder + kSep + kHostSubTwo.substr(0, 9), std::u16string()},
      {kEmbedder + kSep + kHostSubTwo.substr(0, 10), kHostSubTwo.substr(10)},

      // Typing something non-match after an inline autocompletion should stop
      // the inline autocompletion from appearing.
      {kAbout + kSep + kHostB.substr(0, 2) + u"/", std::u16string()},
      {kAbout + kSep + kHostB.substr(0, 2) + u"a", std::u16string()},
      {kAbout + kSep + kHostB.substr(0, 2) + u"+", std::u16string()},
  };

  ACMatches matches;
  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("case %" PRIuS ": %s", i,
                                    base::UTF16ToUTF8(cases[i].input).c_str()));
    AutocompleteInput input(cases[i].input, metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    matches = provider_->matches();
    if (cases[i].expected_inline_autocompletion.empty()) {
      // If we're not expecting an inline autocompletion, make sure that no
      // matches are allowed_to_be_default.
      for (size_t j = 0; j < matches.size(); ++j) {
        EXPECT_LT(matches[j].relevance,
                  HistoryURLProvider::kScoreForWhatYouTypedResult);
        EXPECT_FALSE(matches[j].allowed_to_be_default_match);
      }
    } else {
      // If we are expecting an inline autocompletion, confirm that one and only
      // one of the matches is marked as allowed_to_be_default and that its
      // inline autocompletion is equal to the expected inline autocompletion.
      ASSERT_FALSE(matches.empty());
      size_t default_match_index = matches.size();
      for (size_t j = 0; j < matches.size(); ++j) {
        // If we already found a match that is allowed_to_be_default, ensure
        // that subsequent matches are NOT marked as allowed_to_be_default.
        if (default_match_index < matches.size()) {
          ASSERT_FALSE(matches[j].allowed_to_be_default_match)
              << "Only one match should be allowed to be the default match.";
        } else if (matches[j].allowed_to_be_default_match) {
          default_match_index = j;
        }
      }
      ASSERT_LT(default_match_index, matches.size())
          << "One match should be marked as allowed to be default but none is.";
      EXPECT_GT(matches[default_match_index].relevance,
                HistoryURLProvider::kScoreForWhatYouTypedResult);
      EXPECT_EQ(cases[i].expected_inline_autocompletion,
                matches[default_match_index].inline_autocompletion);
    }
  }
}
