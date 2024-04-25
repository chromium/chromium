// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/calculator_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/provider_state_service.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/test_scheme_classifier.h"

namespace {
class FakeSearchProvider : public SearchProvider {
 public:
  using SearchProvider::done_;
  using SearchProvider::matches_;
  using SearchProvider::SearchProvider;

 protected:
  ~FakeSearchProvider() override = default;
};

struct SearchMatch {
  std::u16string contents;
  bool is_calc = true;
};

}  // namespace

class CalculatorProviderTest : public testing::Test,
                               public AutocompleteProviderListener {
 protected:
  CalculatorProviderTest() {
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    search_provider_ = new FakeSearchProvider(client_.get(), this);
    calculator_provider_ =
        new CalculatorProvider(client_.get(), this, search_provider_.get());
  }

  ~CalculatorProviderTest() override {
    client_->GetProviderStateService()->calculator_provider_cache.clear();
  }

  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override {}

  // Helper to simulate the search and calc providers starting. The search
  // provider will complete if `search_completes_sync` is true. It will have
  // `search_matches` regardless of whether it completes.
  void StartAutocompletion(bool search_completes_sync,
                           std::vector<SearchMatch> search_matches,
                           AutocompleteInput input) {
    search_provider_->matches_.clear();
    for (const auto& search_match : search_matches) {
      AutocompleteMatch match(search_provider_.get(), 1000, true,
                              search_match.is_calc
                                  ? AutocompleteMatchType::CALCULATOR
                                  : AutocompleteMatchType::SEARCH_SUGGEST);
      match.contents = search_match.contents;
      search_provider_->matches_.push_back(match);
    }
    search_provider_->done_ = search_completes_sync;
    calculator_provider_->Start(input, false);
  }

  // Helper to simulate the search and calc providers starting. The search
  // provider will complete with either a single non-calc match, or an
  // additional second calc match.
  void TypeAndStartAutocompletion(std::u16string input_text,
                                  bool include_calc) {
    std::vector<SearchMatch> search_matches = {{input_text, false}};
    if (include_calc)
      search_matches.push_back({input_text});
    AutocompleteInput input{input_text, metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier()};
    StartAutocompletion(true, search_matches, input);
  }

  std::vector<std::u16string> GetCalcMatches() {
    std::vector<std::u16string> contents;
    for (const auto& m : calculator_provider_->matches())
      contents.push_back(m.contents);
    return contents;
  }

  base::test::ScopedFeatureList feature_list_{
      omnibox_feature_configs::CalcProvider::kCalcProvider};
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<FakeSearchProvider> search_provider_;
  scoped_refptr<CalculatorProvider> calculator_provider_;
  // Used to simulate time passing.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(CalculatorProviderTest, SkipSyncInputs) {
  // Returns early when `omit_asynchronous_matches()` is true.
  AutocompleteInput input;
  input.set_omit_asynchronous_matches(true);
  StartAutocompletion(true, {{u"0+1=1"}}, input);
  EXPECT_TRUE(calculator_provider_->done());
}

TEST_F(CalculatorProviderTest, RunSynclyForSyncSearchProvider) {
  // If search provider finished syncly, so should the calc provider.

  // Search completes syncly with 0 matches.
  StartAutocompletion(true, {}, {});
  EXPECT_TRUE(calculator_provider_->done());
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre());

  // Search completes syncly with matches.
  StartAutocompletion(true, {{u"search", false}, {u"0+1=1"}, {u"0+2=2"}}, {});
  EXPECT_TRUE(calculator_provider_->done());
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"0+1=1", u"0+2=2"));
}

TEST_F(CalculatorProviderTest, RunAsynclyForAsyncSearchProvider) {
  // If search provider finished asyncly, so should the calc provider.

  // Start but don't complete search.
  StartAutocompletion(false, {}, {});
  EXPECT_FALSE(calculator_provider_->done());
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre());

  // Search completes with 0 matches.
  search_provider_->done_ = true;
  search_provider_->NotifyListeners(false);
  EXPECT_TRUE(calculator_provider_->done());
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre());

  // Start but don't complete search.
  StartAutocompletion(false, {{u"search", false}, {u"0+1=1"}, {u"0+2=2"}}, {});
  EXPECT_FALSE(calculator_provider_->done());
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre());

  // Complete search with matches
  search_provider_->done_ = true;
  search_provider_->NotifyListeners(false);
  EXPECT_TRUE(calculator_provider_->done());
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"0+1=1", u"0+2=2"));
}

TEST_F(CalculatorProviderTest, CachePreviousCalcSuggestions) {
  // Search has a calc suggestion. It should be shown.
  StartAutocompletion(true, {{u"0+1=1"}}, {});
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"0+1=1"));

  // Search has a different calc suggestion. Both the old and new should be
  // shown.
  StartAutocompletion(true, {{u"0+2=2"}}, {});
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"0+1=1", u"0+2=2"));

  // Search doesn't have a calc suggestion. The old 2 should be shown.
  StartAutocompletion(true, {{u"search", false}}, {});
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"0+1=1", u"0+2=2"));

  // Calc suggestions should expire after 1 hour.
  task_environment_.FastForwardBy(base::Minutes(59));
  StartAutocompletion(true, {{u"search", false}}, {});
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"0+1=1", u"0+2=2"));
  task_environment_.FastForwardBy(base::Minutes(60));
  StartAutocompletion(true, {{u"search", false}}, {});
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre());
}

TEST_F(CalculatorProviderTest, MaxCacheSize) {
  StartAutocompletion(true, {{u"1=1"}, {u"2=2"}, {u"3=3"}, {u"4=4"}}, {});
  EXPECT_THAT(GetCalcMatches(),
              testing::ElementsAre(u"1=1", u"2=2", u"3=3", u"4=4"));

  // "2=2" is omitted, because it's the oldest. "1=1" is refreshed below.
  StartAutocompletion(true, {{u"1=1"}, {u"5=5"}, {u"6=6"}, {u"7=7"}}, {});
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"3=3", u"4=4", u"1=1",
                                                     u"5=5", u"6=6", u"7=7"));
}

TEST_F(CalculatorProviderTest, DedupeAndCoalesceIntermediateInputs) {
  // When typing 1+2+3+4, shouldn't see intermediate matches (e.g. 1+2).
  TypeAndStartAutocompletion(u"1", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre());
  TypeAndStartAutocompletion(u"1+", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre());
  TypeAndStartAutocompletion(u"1+2", true);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+2"));
  TypeAndStartAutocompletion(u"1+2+", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+2"));
  TypeAndStartAutocompletion(u"1+2+3", true);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+2+3"));
  TypeAndStartAutocompletion(u"1+2+3+", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+2+3"));
  TypeAndStartAutocompletion(u"1+2+3+4", true);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+2+3+4"));

  // When backspace and retyping, shouldn't show duplicates.
  TypeAndStartAutocompletion(u"1+2+3+", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+2+3+4"));
  TypeAndStartAutocompletion(u"1+2+3+4", true);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+2+3+4"));

  // When backspacing and retyping, should show non-duplicates.
  TypeAndStartAutocompletion(u"1+", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+2+3+4"));
  TypeAndStartAutocompletion(u"1+2", true);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+2+3+4", u"1+2"));
}

TEST_F(CalculatorProviderTest, ShowCalcSuggestionsForCorrectInputs) {
  // Should stop showing calc suggestions if there have been no recent ones.
  TypeAndStartAutocompletion(u"1+1", true);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+1"));
  TypeAndStartAutocompletion(u"2+2", true);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+1", u"2+2"));
  TypeAndStartAutocompletion(u"2+2+", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+1", u"2+2"));
  TypeAndStartAutocompletion(u"2+2+x", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+1", u"2+2"));
  TypeAndStartAutocompletion(u"2+2+x+", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+1", u"2+2"));
  // The last calc suggestion was too old. The cached ones shouldn't be shown.
  TypeAndStartAutocompletion(u"2+2+x+y", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre());
  // A new calc suggestion should resurface the old ones too
  TypeAndStartAutocompletion(u"3+3", true);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+1", u"2+2", u"3+3"));
  // Cached suggestions should be shown when backspacing as well.
  TypeAndStartAutocompletion(u"3+", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre(u"1+1", u"2+2", u"3+3"));
  // But not when typing an input dissimilar from the last input that had a calc
  // suggestion.
  TypeAndStartAutocompletion(u"3-", false);
  EXPECT_THAT(GetCalcMatches(), testing::ElementsAre());
}
