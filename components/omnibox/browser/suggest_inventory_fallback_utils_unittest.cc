// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggest_inventory_fallback_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/suggest_inventory.pb.h"

namespace omnibox {
namespace {

using OEP = ::metrics::OmniboxEventProto;

class SuggestInventoryFallbackUtilsTest : public testing::Test {
 public:
  SuggestInventoryFallbackUtilsTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  MockAutocompleteProviderClient client_;
};

TEST_F(SuggestInventoryFallbackUtilsTest,
       GetFallbackPromptsForSuggestInventory_Brainstorm) {
  auto prompts = GetFallbackPromptsForSuggestInventory(
      SuggestInventory::SUGGEST_INVENTORY_BRAINSTORM,
      kDefaultFallbackNumSuggestions);
  EXPECT_EQ(prompts.size(), kDefaultFallbackNumSuggestions);
  for (const auto& prompt : prompts) {
    EXPECT_FALSE(prompt.first.empty());
    EXPECT_FALSE(prompt.second.empty());
  }
}

TEST_F(SuggestInventoryFallbackUtilsTest,
       GetFallbackPromptsForSuggestInventory_HelpMeLearn) {
  auto prompts = GetFallbackPromptsForSuggestInventory(
      SuggestInventory::SUGGEST_INVENTORY_HELP_ME_LEARN,
      kDefaultFallbackNumSuggestions);
  EXPECT_EQ(prompts.size(), kDefaultFallbackNumSuggestions);
  for (const auto& prompt : prompts) {
    EXPECT_FALSE(prompt.first.empty());
    EXPECT_FALSE(prompt.second.empty());
  }
}

TEST_F(SuggestInventoryFallbackUtilsTest,
       GetFallbackPromptsForSuggestInventory_WriteOrEdit) {
  auto prompts = GetFallbackPromptsForSuggestInventory(
      SuggestInventory::SUGGEST_INVENTORY_WRITE_OR_EDIT,
      kDefaultFallbackNumSuggestions);
  EXPECT_EQ(prompts.size(), kDefaultFallbackNumSuggestions);
  for (const auto& prompt : prompts) {
    EXPECT_FALSE(prompt.first.empty());
    EXPECT_FALSE(prompt.second.empty());
  }
}

TEST_F(SuggestInventoryFallbackUtilsTest,
       GetFallbackPromptsForSuggestInventory_Unsupported) {
  auto prompts = GetFallbackPromptsForSuggestInventory(
      SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
      kDefaultFallbackNumSuggestions);
  EXPECT_TRUE(prompts.empty());
}

TEST_F(SuggestInventoryFallbackUtilsTest,
       GetFallbackPromptsForSuggestInventory_MaxCountLimit) {
  const size_t kMaxCount = 2;
  auto prompts = GetFallbackPromptsForSuggestInventory(
      SuggestInventory::SUGGEST_INVENTORY_BRAINSTORM,
      /*num_suggestions=*/kMaxCount);
  EXPECT_EQ(prompts.size(), kMaxCount);
}

TEST_F(SuggestInventoryFallbackUtilsTest,
       MaybeCreateFallbackMatchesForSuggestInventory) {
  client_.set_template_url_service(
      search_engines_test_environment_.template_url_service());

  AutocompleteInput input(u"", OEP::NTP_COMPOSEBOX, TestSchemeClassifier());
  input.set_focus_type(::metrics::OmniboxFocusType::INTERACTION_FOCUS);
  input.set_suggest_inventory(SuggestInventory::SUGGEST_INVENTORY_BRAINSTORM);

  auto matches = MaybeCreateFallbackMatchesForSuggestInventory(
      /*provider=*/nullptr, &client_, input, kDefaultFallbackNumSuggestions);

  EXPECT_EQ(matches.size(), kDefaultFallbackNumSuggestions);
  int expected_relevance = kDefaultFallbackSuggestRelevance;
  for (const auto& match : matches) {
    EXPECT_EQ(match.type, AutocompleteMatchType::SEARCH_SUGGEST);
    EXPECT_EQ(match.relevance, expected_relevance--);
    EXPECT_FALSE(match.keyword.empty());
    EXPECT_EQ(match.keyword,
              search_engines_test_environment_.template_url_service()
                  ->GetDefaultSearchProvider()
                  ->keyword());
    EXPECT_FALSE(match.contents.empty());
    EXPECT_FALSE(match.fill_into_edit.empty());
    EXPECT_NE(match.fill_into_edit, match.contents);
    ASSERT_TRUE(match.search_terms_args);
    EXPECT_EQ(match.search_terms_args->search_terms, match.fill_into_edit);
    EXPECT_EQ(match.search_terms_args->page_classification,
              OEP::NTP_COMPOSEBOX);
    EXPECT_TRUE(match.destination_url.is_valid());
  }
}

}  // namespace
}  // namespace omnibox
