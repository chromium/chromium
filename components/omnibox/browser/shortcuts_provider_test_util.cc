// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/shortcuts_provider_test_util.h"

#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/omnibox/browser/shortcuts_provider.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"

TestShortcutData::TestShortcutData(
    std::string guid,
    std::string text,
    std::string fill_into_edit,
    std::string destination_url,
    AutocompleteMatch::DocumentType document_type,
    std::string contents,
    std::string contents_class,
    std::string description,
    std::string description_class,
    ui::PageTransition transition,
    AutocompleteMatch::Type type,
    std::string keyword,
    int days_from_now,
    int number_of_hits) {
  this->guid = guid;
  this->text = text;
  this->fill_into_edit = fill_into_edit;
  this->destination_url = destination_url;
  this->document_type = document_type;
  this->contents = contents;
  this->contents_class = contents_class;
  this->description = description;
  this->description_class = description_class;
  this->transition = transition;
  this->type = type;
  this->keyword = keyword;
  this->days_from_now = days_from_now;
  this->number_of_hits = number_of_hits;
}

TestShortcutData::~TestShortcutData() = default;

void PopulateShortcutsBackendWithTestData(
    scoped_refptr<ShortcutsBackend> backend,
    TestShortcutData* db,
    size_t db_size) {
  size_t expected_size = backend->shortcuts_map().size() + db_size;
  for (size_t i = 0; i < db_size; ++i) {
    const TestShortcutData& cur = db[i];
    ShortcutsDatabase::Shortcut shortcut(
        cur.guid, base::ASCIIToUTF16(cur.text),
        ShortcutsDatabase::Shortcut::MatchCore(
            base::ASCIIToUTF16(cur.fill_into_edit), GURL(cur.destination_url),
            cur.document_type, base::ASCIIToUTF16(cur.contents),
            cur.contents_class, base::ASCIIToUTF16(cur.description),
            cur.description_class, cur.transition, cur.type,
            base::ASCIIToUTF16(cur.keyword)),
        base::Time::Now() - base::Days(cur.days_from_now), cur.number_of_hits);
    backend->AddShortcut(shortcut);
  }
  EXPECT_EQ(expected_size, backend->shortcuts_map().size());
}

void RunShortcutsProviderTest(
    scoped_refptr<ShortcutsProvider> provider,
    const std::u16string text,
    bool prevent_inline_autocomplete,
    const std::vector<ExpectedURLAndAllowedToBeDefault>& expected_urls,
    std::string expected_top_result,
    std::u16string top_result_inline_autocompletion) {
  AutocompleteInput input(text, metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_prevent_inline_autocomplete(prevent_inline_autocomplete);
  RunShortcutsProviderTest(provider, input, expected_urls, expected_top_result,
                           top_result_inline_autocompletion);
}

void RunShortcutsProviderTest(
    scoped_refptr<ShortcutsProvider> provider,
    const AutocompleteInput& input,
    const std::vector<ExpectedURLAndAllowedToBeDefault>& expected_urls,
    std::string expected_top_result,
    std::u16string top_result_inline_autocompletion) {
  base::RunLoop().RunUntilIdle();
  provider->Start(input, false);
  EXPECT_TRUE(provider->done());

  ACMatches ac_matches = provider->matches();

  std::string debug =
      base::StringPrintf("Input [%s], prevent inline [%d], matches:\n",
                         base::UTF16ToUTF8(input.text()).c_str(),
                         input.prevent_inline_autocomplete());
  for (auto match : ac_matches) {
    debug += base::StringPrintf("  URL [%s], default [%d]\n",
                                match.destination_url.spec().c_str(),
                                match.allowed_to_be_default_match);
  }

  // We should have gotten back at most
  // AutocompleteProvider::provider_max_matches().
  EXPECT_LE(ac_matches.size(), provider->provider_max_matches()) << debug;

  // If the number of expected and actual matches aren't equal then we need
  // test no further, but let's do anyway so that we know which URLs failed.
  EXPECT_EQ(expected_urls.size(), ac_matches.size()) << debug;

  for (const auto& expected_url : expected_urls) {
    EXPECT_TRUE(base::ranges::any_of(
        ac_matches,
        [&expected_url](const AutocompleteMatch& match) {
          return expected_url.first == match.destination_url.spec() &&
                 expected_url.second == match.allowed_to_be_default_match;
        }))
        << debug
        << base::StringPrintf("Expected URL [%s], default [%d]\n",
                              expected_url.first.c_str(), expected_url.second);
  }

  // See if we got the expected top scorer.
  if (!ac_matches.empty()) {
    std::partial_sort(ac_matches.begin(), ac_matches.begin() + 1,
                      ac_matches.end(), AutocompleteMatch::MoreRelevant);
    EXPECT_EQ(expected_top_result, ac_matches[0].destination_url.spec())
        << debug;
    EXPECT_EQ(top_result_inline_autocompletion,
              ac_matches[0].inline_autocompletion)
        << debug;
  }
}
