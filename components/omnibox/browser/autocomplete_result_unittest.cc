// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/autocomplete_result.h"

#include <stddef.h>

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/fake_tab_matcher.h"
#include "components/omnibox/browser/intranet_redirector_state.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/variations_associated_data.h"
#include "omnibox_focus_type.pb.h"
#include "omnibox_triggered_feature_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/entity_info.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/device_form_factor.h"

using metrics::OmniboxEventProto;

namespace {
class FakeOmniboxAction : public OmniboxAction {
 public:
  explicit FakeOmniboxAction(OmniboxActionId id)
      : OmniboxAction(LabelStrings(u"", u"", u"", u""), GURL{}), id_(id) {}
  OmniboxActionId ActionId() const override { return id_; }

 private:
  ~FakeOmniboxAction() override = default;
  OmniboxActionId id_{};
};

struct AutocompleteMatchTestData {
  std::string destination_url;
  AutocompleteMatch::Type type;
};

// Adds |count| AutocompleteMatches to |matches|.
template <typename T>
void PopulateAutocompleteMatchesFromTestData(const T* data,
                                             size_t count,
                                             ACMatches* matches) {
  static_assert(std::is_base_of<AutocompleteMatchTestData, T>::value,
                "T must derive from AutocompleteMatchTestData");
  ASSERT_TRUE(matches != nullptr);
  for (size_t i = 0; i < count; ++i) {
    AutocompleteMatch match;
    match.destination_url = GURL(data[i].destination_url);
    match.relevance =
        matches->empty() ? 1300 : (matches->back().relevance - 100);
    match.allowed_to_be_default_match = true;
    match.type = data[i].type;
    matches->push_back(match);
  }
}

// Basic match representation for testing `MaybeCullTailSuggestions()`.
// Defined externally to allow for `PrintTo()`.
struct CullTailTestMatch {
  std::u16string id;
  AutocompleteMatchType::Type type;
  bool allowed_default;

  bool operator==(const CullTailTestMatch& other) const {
    return id == other.id && type == other.type &&
           allowed_default == other.allowed_default;
  }

  // To help `EXPECT_THAT` pretty print `CullTailTestMatch`s.
  friend void PrintTo(const CullTailTestMatch& match, std::ostream* os) {
    *os << match.id << " " << match.type << " " << match.allowed_default;
  }
};

}  // namespace

class AutocompleteResultForTesting : public AutocompleteResult {
 public:
  using AutocompleteResult::DemoteOnDeviceSearchSuggestions;
  using AutocompleteResult::matches_;
  using AutocompleteResult::MaybeCullTailSuggestions;
};

class AutocompleteResultTest : public testing::Test {
 public:
  struct TestData {
    // Used to build a URL for the AutocompleteMatch. The URL becomes
    // "http://" + ('a' + |url_id|) (e.g. an ID of 2 yields "http://c").
    int url_id;

    // ID of the provider.
    int provider_id;

    // Relevance score.
    int relevance;

    // Allowed to be default match status.
    bool allowed_to_be_default_match;

    // Duplicate matches.
    std::vector<AutocompleteMatch> duplicate_matches;

    // Type of the match
    AutocompleteMatchType::Type type{AutocompleteMatchType::SEARCH_SUGGEST};

    // Suggestion Group ID for this suggestion
    std::optional<omnibox::GroupId> suggestion_group_id;

    // Inline autocompletion.
    std::string inline_autocompletion;

    IphType iph_type = IphType::kNone;
  };

  AutocompleteResultTest() {
    variations::testing::ClearAllVariationParams();

    // Create the list of mock providers. 5 is enough.
    mock_provider_list_.push_back(new FakeAutocompleteProvider(
        AutocompleteProvider::Type::TYPE_HISTORY_QUICK));
    mock_provider_list_.push_back(new FakeAutocompleteProvider(
        AutocompleteProvider::Type::TYPE_HISTORY_URL));
    mock_provider_list_.push_back(
        new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_SEARCH));
    mock_provider_list_.push_back(new FakeAutocompleteProvider(
        AutocompleteProvider::Type::TYPE_ON_DEVICE_HEAD));
    mock_provider_list_.push_back(new FakeAutocompleteProvider(
        AutocompleteProvider::Type::TYPE_FEATURED_SEARCH));

    for (const auto& provider : mock_provider_list_)
      provider->done_ = false;

    template_url_service().Load();
  }
  AutocompleteResultTest(const AutocompleteResultTest&) = delete;
  AutocompleteResultTest& operator=(const AutocompleteResultTest&) = delete;

  void TearDown() override { task_environment_.RunUntilIdle(); }

  // Converts `TestData` to `AutocompleteMatch`.
  AutocompleteMatch PopulateAutocompleteMatch(const TestData& data);

  // Adds |count| AutocompleteMatches to |matches|.
  void PopulateAutocompleteMatches(const TestData* data,
                                   size_t count,
                                   ACMatches* matches);
  ACMatches PopulateAutocompleteMatches(const std::vector<TestData>& data);

  // Asserts that |result| has |expected_count| matches matching |expected|.
  void AssertResultMatches(const AutocompleteResult& result,
                           base::span<const TestData> expected);

  void AssertMatch(AutocompleteMatch match,
                   const TestData& expected_match_data,
                   int i);

  // Creates an AutocompleteResult from |last| and |current|. The two are
  // merged by |TransferOldMatches| and compared by |AssertResultMatches|.
  void RunTransferOldMatchesTest(const TestData* last,
                                 size_t last_size,
                                 const TestData* current,
                                 size_t current_size,
                                 const TestData* expected,
                                 size_t expected_size);
  void RunTransferOldMatchesTest(const TestData* last,
                                 size_t last_size,
                                 const TestData* current,
                                 size_t current_size,
                                 const TestData* expected,
                                 size_t expected_size,
                                 AutocompleteInput input);

  void SortMatchesAndVerifyOrder(
      const std::string& input_text,
      OmniboxEventProto::PageClassification page_classification,
      const ACMatches& matches,
      const std::vector<size_t>& expected_order,
      const AutocompleteMatchTestData data[]);

  // Returns a (mock) AutocompleteProvider of given |provider_id|.
  FakeAutocompleteProvider* GetProvider(int provider_id) {
    EXPECT_LT(provider_id, static_cast<int>(mock_provider_list_.size()));
    return mock_provider_list_[provider_id].get();
  }

  OmniboxTriggeredFeatureService* triggered_feature_service() {
    return &triggered_feature_service_;
  }

  TemplateURLService& template_url_service() {
    return *search_engines_test_environment_.template_url_service();
  }

 protected:
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;

 private:
  base::test::TaskEnvironment task_environment_;
  OmniboxTriggeredFeatureService triggered_feature_service_;

  // For every provider mentioned in TestData, we need a mock provider.
  std::vector<scoped_refptr<FakeAutocompleteProvider>> mock_provider_list_;
};

AutocompleteMatch AutocompleteResultTest::PopulateAutocompleteMatch(
    const TestData& data) {
  AutocompleteMatch match;
  match.provider = GetProvider(data.provider_id);
  match.type = data.type;
  match.fill_into_edit = base::NumberToString16(data.url_id);
  std::string url_id(1, data.url_id + 'a');
  match.destination_url = GURL("http://" + url_id);
  match.relevance = data.relevance;
  match.allowed_to_be_default_match = data.allowed_to_be_default_match;
  match.duplicate_matches = data.duplicate_matches;
  if (data.suggestion_group_id.has_value()) {
    match.suggestion_group_id = data.suggestion_group_id.value();
  }
  match.inline_autocompletion = base::UTF8ToUTF16(data.inline_autocompletion);
  match.iph_type = data.iph_type;
  return match;
}

void AutocompleteResultTest::PopulateAutocompleteMatches(const TestData* data,
                                                         size_t count,
                                                         ACMatches* matches) {
  for (size_t i = 0; i < count; ++i)
    matches->push_back(PopulateAutocompleteMatch(data[i]));
}

ACMatches AutocompleteResultTest::PopulateAutocompleteMatches(
    const std::vector<TestData>& data) {
  ACMatches matches;
  for (const auto& d : data)
    matches.push_back(PopulateAutocompleteMatch(d));
  return matches;
}

void AutocompleteResultTest::AssertResultMatches(
    const AutocompleteResult& result,
    base::span<const TestData> expected) {
  ASSERT_EQ(expected.size(), result.size());
  for (size_t i = 0; i < expected.size(); ++i)
    AssertMatch(*(result.begin() + i), expected[i], i);
}

void AutocompleteResultTest::AssertMatch(AutocompleteMatch match,
                                         const TestData& expected_match_data,
                                         int i) {
  AutocompleteMatch expected_match =
      PopulateAutocompleteMatch(expected_match_data);
  EXPECT_EQ(expected_match.provider, match.provider) << i;
  EXPECT_EQ(expected_match.type, match.type) << i;
  EXPECT_EQ(expected_match.relevance, match.relevance) << i;
  EXPECT_EQ(expected_match.allowed_to_be_default_match,
            match.allowed_to_be_default_match)
      << i;
  EXPECT_EQ(expected_match.destination_url.spec(), match.destination_url.spec())
      << i;
  EXPECT_EQ(expected_match.inline_autocompletion, match.inline_autocompletion)
      << i;
}

void AutocompleteResultTest::RunTransferOldMatchesTest(const TestData* last,
                                                       size_t last_size,
                                                       const TestData* current,
                                                       size_t current_size,
                                                       const TestData* expected,
                                                       size_t expected_size) {
  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  RunTransferOldMatchesTest(last, last_size, current, current_size, expected,
                            expected_size, input);
}

void AutocompleteResultTest::RunTransferOldMatchesTest(
    const TestData* last,
    size_t last_size,
    const TestData* current,
    size_t current_size,
    const TestData* expected,
    size_t expected_size,
    AutocompleteInput input) {
  ACMatches last_matches;
  PopulateAutocompleteMatches(last, last_size, &last_matches);
  AutocompleteResult last_result;
  last_result.AppendMatches(last_matches);
  last_result.SortAndCull(input, &template_url_service(),
                          triggered_feature_service());

  ACMatches current_matches;
  PopulateAutocompleteMatches(current, current_size, &current_matches);
  AutocompleteResult current_result;
  current_result.AppendMatches(current_matches);
  current_result.SortAndCull(input, &template_url_service(),
                             triggered_feature_service());
  current_result.TransferOldMatches(input, &last_result);
  current_result.SortAndCull(input, &template_url_service(),
                             triggered_feature_service());

  AssertResultMatches(current_result, {expected, expected_size});
}

void AutocompleteResultTest::SortMatchesAndVerifyOrder(
    const std::string& input_text,
    OmniboxEventProto::PageClassification page_classification,
    const ACMatches& matches,
    const std::vector<size_t>& expected_order,
    const AutocompleteMatchTestData data[]) {
  AutocompleteInput input(base::ASCIIToUTF16(input_text), page_classification,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  ASSERT_EQ(expected_order.size(), result.size());
  for (size_t i = 0; i < expected_order.size(); ++i) {
    EXPECT_EQ(data[expected_order[i]].destination_url,
              result.match_at(i)->destination_url.spec())
        << "Unexpected item at position " << i;
  }
}

// Assertion testing for AutocompleteResult::SwapMatchesWith.
TEST_F(AutocompleteResultTest, SwapMatches) {
  AutocompleteResult r1;
  AutocompleteResult r2;

  // Swap with empty shouldn't do anything interesting.
  r1.SwapMatchesWith(&r2);
  EXPECT_FALSE(r1.default_match());
  EXPECT_FALSE(r2.default_match());

  // Swap with a single match.
  ACMatches matches;
  AutocompleteMatch match;
  match.relevance = 1;
  match.allowed_to_be_default_match = true;
  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  matches.push_back(match);
  r1.AppendMatches(matches);
  r1.SortAndCull(input, &template_url_service(), triggered_feature_service());
  EXPECT_TRUE(r1.default_match());
  EXPECT_EQ(&*r1.begin(), r1.default_match());

  r1.SwapMatchesWith(&r2);
  EXPECT_TRUE(r1.empty());
  EXPECT_FALSE(r1.default_match());
  ASSERT_FALSE(r2.empty());
  EXPECT_TRUE(r2.default_match());
  EXPECT_EQ(&*r2.begin(), r2.default_match());
}

TEST_F(AutocompleteResultTest, AlternateNavUrl) {
  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  FakeAutocompleteProviderClient client;
  reinterpret_cast<TestingPrefServiceSimple*>(client.GetLocalState())
      ->registry()
      ->RegisterIntegerPref(omnibox::kIntranetRedirectBehavior, 0);

  // Against search matches, we should not generate an alternate nav URL, unless
  // overriden by policy, tested in AlternateNavUrl_IntranetRedirectPolicy
  // below.
  {
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::SEARCH_SUGGEST;
    match.destination_url = GURL("http://www.foo.com/s?q=foo");
    GURL alternate_nav_url =
        AutocompleteResult::ComputeAlternateNavUrl(input, match, &client);
    EXPECT_FALSE(alternate_nav_url.is_valid());
  }

  // Against matching URL matches, we should never generate an alternate nav
  // URL.
  {
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::SEARCH_SUGGEST;
    match.destination_url = GURL("http://a/");
    GURL alternate_nav_url =
        AutocompleteResult::ComputeAlternateNavUrl(input, match, &client);
    EXPECT_FALSE(alternate_nav_url.is_valid());
  }
}

TEST_F(AutocompleteResultTest, AlternateNavUrl_IntranetRedirectPolicy) {
  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  FakeAutocompleteProviderClient client;
  reinterpret_cast<TestingPrefServiceSimple*>(client.GetLocalState())
      ->registry()
      ->RegisterIntegerPref(omnibox::kIntranetRedirectBehavior, 0);

  // Allow alternate nav URLs when policy allows.
  {
    client.GetLocalState()->SetInteger(
        omnibox::kIntranetRedirectBehavior,
        static_cast<int>(omnibox::IntranetRedirectorBehavior::
                             ENABLE_INTERCEPTION_CHECKS_AND_INFOBARS));

    AutocompleteMatch match;
    match.type = AutocompleteMatchType::SEARCH_SUGGEST;
    match.destination_url = GURL("http://www.foo.com/s?q=foo");
    GURL alternate_nav_url =
        AutocompleteResult::ComputeAlternateNavUrl(input, match, &client);
    EXPECT_EQ("http://a/", alternate_nav_url.spec());
  }

  // Disallow alternate nav URLs when policy disallows.
  {
    client.GetLocalState()->SetInteger(
        omnibox::kIntranetRedirectBehavior,
        static_cast<int>(omnibox::IntranetRedirectorBehavior::DISABLE_FEATURE));

    AutocompleteMatch match;
    match.type = AutocompleteMatchType::SEARCH_SUGGEST;
    match.destination_url = GURL("http://www.foo.com/s?q=foo");
    GURL alternate_nav_url =
        AutocompleteResult::ComputeAlternateNavUrl(input, match, &client);
    EXPECT_FALSE(alternate_nav_url.is_valid());
  }
}

// Tests that if the new results have a lower max relevance score than last,
// any copied results have their relevance shifted down.
TEST_F(AutocompleteResultTest, TransferOldMatches) {
  TestData last[] = {
      {0, 1, 1000, true},
      {1, 1, 500, true},
  };
  TestData current[] = {
      {2, 1, 400, true},
  };
  TestData result[] = {
      {2, 1, 400, true},
      {1, 1, 399, false},  // transferred matches aren't allowed to be default.
  };

  ASSERT_NO_FATAL_FAILURE(RunTransferOldMatchesTest(last, std::size(last),
                                                    current, std::size(current),
                                                    result, std::size(result)));
}

// Tests that if the new results have a lower max relevance score than last,
// any copied results have their relevance shifted down when the allowed to
// be default constraint comes into play.
TEST_F(AutocompleteResultTest, TransferOldMatchesAllowedToBeDefault) {
  TestData last[] = {
      {0, 1, 1300, true},
      {1, 1, 1200, true},
      {2, 1, 1100, true},
  };
  TestData current[] = {
      {3, 1, 1000, false},
      {4, 1, 900, true},
  };
  // The expected results are out of relevance order because the top-scoring
  // allowed to be default match is always pulled to the top.
  TestData result[] = {
      {4, 1, 900, true},
      {3, 1, 1000, false},
      {2, 1, 899, false},  // transferred matches aren't allowed to be default.
  };

  ASSERT_NO_FATAL_FAILURE(RunTransferOldMatchesTest(last, std::size(last),
                                                    current, std::size(current),
                                                    result, std::size(result)));
}

// Tests |TransferOldMatches()| with an |AutocompleteInput| with
// |prevent_inline_autocomplete| set to true. Noteworthy, expect that resulting
// matches must have effectively empty autocompletions; i.e. either empty
// |inline_autocompletion|, or false |allowed_to_be_default|. Tests all 12
// combinations of 1) last match has a lower or higher relevance than current
// match, 2) last match was allowed to be default, 3) last match had
// autocompletion, and 4) current match is allowed to be default.
TEST_F(AutocompleteResultTest,
       TransferOldMatchesAllowedToBeDefaultWithPreventInlineAutocompletion) {
  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_prevent_inline_autocomplete(true);

  {
    SCOPED_TRACE(
        "Current matches not allowed to be default and scored higher.");
    // 1) `allowed_to_be_default` should be true only for `last` matches without
    //    autocompletion.
    // 2) `allowed_to_be_default` should not be set true for previously not
    //     allowed matches, even if they don't have inline autocompletion.
    // 3) When `allowed_to_be_default` is false, `current` matches should be
    //    preferred as they're scored higher.
    // clang-format off
    TestData last[] = {
        {0, 1, 1020, true, {}, AutocompleteMatchType::SEARCH_SUGGEST, {}, "autocompletion"},
        {1, 1, 1010, true},
        {2, 1, 1000, false},
    };
    TestData current[] = {
        {0, 2, 1520, false},
        {1, 2, 1510, false},
        {2, 2, 1500, false},
    };
    TestData result[] = {
        {1, 1, 1510, true},
        {0, 2, 1520, false},
        {2, 2, 1500, false},
    };
    // clang-format on

    ASSERT_NO_FATAL_FAILURE(RunTransferOldMatchesTest(
        last, std::size(last), current, std::size(current), result,
        std::size(result), input));
  }

  {
    SCOPED_TRACE("Current matches not allowed to be default and scored lower.");
    // Similar to above, except `last` matches should be preferred in deduping
    // as they're scored higher.
    // clang-format off
    TestData last[] = {
        {0, 1, 1020, true, {}, AutocompleteMatchType::SEARCH_SUGGEST, {}, "autocompletion"},
        {1, 1, 1010, true},
        {2, 1, 1000, false},
    };
    TestData current[] = {
        // Need a high-scoring current match to avoid demoting last matches.
        {3, 2, 1500, false},
        {0, 2, 520, false},
        {1, 2, 510, false},
        {2, 2, 500, false},
    };
    TestData result[] = {
        {1, 1, 1010, true},
        {3, 2, 1500, false},
        {0, 1, 1020, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, {}, "autocompletion"},
        {2, 1, 1000, false},
    };
    // clang-format on

    ASSERT_NO_FATAL_FAILURE(RunTransferOldMatchesTest(
        last, std::size(last), current, std::size(current), result,
        std::size(result), input));
  }

  {
    SCOPED_TRACE("Current matches allowed to be default and scored higher.");
    // Deduping should prefer the `current` matches as they're both allowed to
    // be default and scored higher.
    // clang-format off
    TestData last[] = {
        {0, 1, 1020, true, {}, AutocompleteMatchType::SEARCH_SUGGEST, {}, "autocompletion"},
        {1, 1, 1010, true},
        {2, 1, 1000, false},
    };
    TestData current[] = {
        {0, 2, 1520, true},
        {1, 2, 1510, true},
        {2, 2, 1500, true},
    };
    TestData result[] = {
        {0, 2, 1520, true},
        {1, 2, 1510, true},
        {2, 2, 1500, true},
    };
    // clang-format on

    ASSERT_NO_FATAL_FAILURE(RunTransferOldMatchesTest(
        last, std::size(last), current, std::size(current), result,
        std::size(result), input));
  }

  {
    SCOPED_TRACE("Current matches allowed to be default and scored lower.");
    // `last` matches still allowed to be default after transferring should be
    // preferred in deduping as they're scored higher. Otherwise, `current`
    // matches should be preferred in deduping as they're allowed to be default.
    // clang-format off
    TestData last[] = {
        {0, 1, 1020, true, {}, AutocompleteMatchType::SEARCH_SUGGEST, {}, "autocompletion"},
        {1, 1, 1010, true},
        {2, 1, 1000, false},
    };
    TestData current[] = {
        // Need a high-scoring current match to avoid demoting last matches.
        {3, 2, 1500, true},
        {0, 2, 520, true},
        {1, 2, 510, true},
        {2, 2, 500, true},
    };
    TestData result[] = {
        {3, 2, 1500, true},
        {0, 2, 1020, true},
        {1, 1, 1010, true},
        {2, 2, 1000, true},
    };
    // clang-format on

    ASSERT_NO_FATAL_FAILURE(RunTransferOldMatchesTest(
        last, std::size(last), current, std::size(current), result,
        std::size(result), input));
  }
}

// Tests that matches are copied correctly from two distinct providers.
TEST_F(AutocompleteResultTest, TransferOldMatchesMultipleProviders) {
  TestData last[] = {
      {0, 1, 1300, false}, {1, 2, 1250, true},  {2, 1, 1200, false},
      {3, 2, 1150, true},  {4, 1, 1100, false},
  };
  TestData current[] = {
      {5, 1, 1000, false},
      {6, 2, 800, true},
      {7, 1, 500, true},
  };
  // The expected results are out of relevance order because the top-scoring
  // allowed to be default match is always pulled to the top.
  TestData result[] = {
      {6, 2, 800, true}, {5, 1, 1000, false}, {3, 2, 799, false},
      {7, 1, 500, true}, {4, 1, 499, false},
  };

  ASSERT_NO_FATAL_FAILURE(RunTransferOldMatchesTest(last, std::size(last),
                                                    current, std::size(current),
                                                    result, std::size(result)));
}

// Tests that matches are copied correctly from two distinct providers when
// one provider doesn't have a current legal default match.
TEST_F(AutocompleteResultTest,
       TransferOldMatchesWithOneProviderWithoutDefault) {
  TestData last[] = {
      {0, 2, 1250, true}, {1, 2, 1150, true}, {2, 1, 900, false},
      {3, 1, 800, false}, {4, 1, 700, false},
  };
  TestData current[] = {
      {5, 1, 1000, true},
      {6, 2, 800, false},
      {7, 1, 500, true},
  };
  TestData result[] = {
      {5, 1, 1000, true}, {1, 2, 999, false}, {6, 2, 800, false},
      {4, 1, 700, false}, {7, 1, 500, true},
  };

  ASSERT_NO_FATAL_FAILURE(RunTransferOldMatchesTest(last, std::size(last),
                                                    current, std::size(current),
                                                    result, std::size(result)));
}

// Tests that transferred matches do not include the specialized match types.
TEST_F(AutocompleteResultTest, TransferOldMatchesSkipsSpecializedSuggestions) {
  TestData last[] = {
      {0, 1, 1000, true, {}, AutocompleteMatchType::TILE_NAVSUGGEST},
      {1, 4, 999, true, {}, AutocompleteMatchType::TILE_SUGGESTION},
      {2, 2, 500, true},
  };
  TestData current[] = {
      {3, 1, 600, true},
  };
  TestData result[] = {
      {3, 1, 600, true},
      {2, 2, 500, false},
  };

  ASSERT_NO_FATAL_FAILURE(RunTransferOldMatchesTest(last, std::size(last),
                                                    current, std::size(current),
                                                    result, std::size(result)));
}

// Tests that transferred matches do not include the specialized match types.
TEST_F(AutocompleteResultTest, TransferOldMatchesSkipDoneProviders) {
  TestData last[] = {
      {0, 1, 500},  // Suggestion from done provider
      {1, 2, 400},  // Suggestion for not-done provider
  };
  TestData current[] = {
      {2, 3, 700},  // Suggestion from done provider
      {3, 4, 600},  // Suggestion for not-done provider
  };
  TestData result[] = {
      {2, 3, 700},  // New suggestion from done provider
      {3, 4, 600},  // New suggestion from not-done provider
      // Skip suggestion `{0, 1, 500}`.
      {1, 2, 400},  // Transferred suggestion from not-done provider
  };

  GetProvider(1)->done_ = true;
  GetProvider(3)->done_ = true;

  ASSERT_NO_FATAL_FAILURE(RunTransferOldMatchesTest(last, std::size(last),
                                                    current, std::size(current),
                                                    result, std::size(result)));
}

// Tests that matches with empty destination URLs aren't treated as duplicates
// and culled.
TEST_F(AutocompleteResultTest, SortAndCullEmptyDestinationURLs) {
  TestData data[] = {
      {1, 1, 500, true},  {0, 1, 1100, true}, {1, 1, 1000, true},
      {0, 1, 1300, true}, {0, 1, 1200, true},
  };

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  matches[1].destination_url = GURL();
  matches[3].destination_url = GURL();
  matches[4].destination_url = GURL();

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  // Of the two results with the same non-empty destination URL, the
  // lower-relevance one should be dropped.  All of the results with empty URLs
  // should be kept.
  ASSERT_EQ(4U, result.size());
  EXPECT_TRUE(result.match_at(0)->destination_url.is_empty());
  EXPECT_EQ(1300, result.match_at(0)->relevance);
  EXPECT_TRUE(result.match_at(1)->destination_url.is_empty());
  EXPECT_EQ(1200, result.match_at(1)->relevance);
  EXPECT_TRUE(result.match_at(2)->destination_url.is_empty());
  EXPECT_EQ(1100, result.match_at(2)->relevance);
  EXPECT_EQ("http://b/", result.match_at(3)->destination_url.spec());
  EXPECT_EQ(1000, result.match_at(3)->relevance);
}

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))
// Tests which remove results only work on desktop.

TEST_F(AutocompleteResultTest, SortAndCullTailSuggestions) {
  // clang-format off
  TestData data[] = {
      {1, 1, 500,  true},
      {2, 1, 1100, false},
      {3, 1, 1000, false},
      {4, 1, 1300, false},
      {5, 1, 1200, false},
  };
  // clang-format on

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  // These will get sorted up, but still removed.
  matches[3].type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
  matches[4].type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  EXPECT_EQ(3UL, result.size());
  EXPECT_NE(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
            result.match_at(0)->type);
  EXPECT_NE(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
            result.match_at(1)->type);
  EXPECT_NE(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
            result.match_at(2)->type);
}

TEST_F(AutocompleteResultTest, SortAndCullKeepDefaultTailSuggestions) {
  // clang-format off
  TestData data[] = {
      {1, 1, 500,  true},
      {2, 1, 1100, false},
      {3, 1, 1000, false},
      {4, 1, 1300, false},
      {5, 1, 1200, false},
  };
  // clang-format on

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  // Make sure that even bad tail suggestions, if the only default match,
  // are kept.
  matches[0].type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
  matches[1].type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
  matches[2].type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  EXPECT_EQ(3UL, result.size());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
            result.match_at(0)->type);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
            result.match_at(1)->type);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
            result.match_at(2)->type);
}

TEST_F(AutocompleteResultTest, SortAndCullKeepMoreDefaultTailSuggestions) {
  // clang-format off
  TestData data[] = {
      {1, 1, 500,  true},   // Low score non-tail default
      {2, 1, 1100, false},  // Tail
      {3, 1, 1000, true},   // Allow a tail suggestion to be the default.
      {4, 1, 1300, false},  // Tail
      {5, 1, 1200, false},  // Tail
  };
  // clang-format on

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  // Make sure that even a bad non-tail default suggestion is kept.
  for (size_t i = 1; i < 5; ++i)
    matches[i].type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  EXPECT_EQ(5UL, result.size());
  // Non-tail default must be first, regardless of score
  EXPECT_NE(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
            result.match_at(0)->type);
  for (size_t i = 1; i < 5; ++i) {
    EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
              result.match_at(i)->type);
    EXPECT_FALSE(result.match_at(i)->allowed_to_be_default_match);
  }
}

TEST_F(AutocompleteResultTest, SortAndCullZeroRelevanceSuggestions) {
  // clang-format off
  TestData data[] = {
      {1, 1, 1000, true},   // A default non-tail suggestion.
      {2, 1, 0,    true},   // A no-relevance default non-tail suggestion.
      {3, 1, 1100, true},   // Default tail
      {4, 1, 1000, false},  // Tail
      {5, 1, 1300, false},  // Tail
      {6, 1, 0,    false},  // No-relevance tail suggestion.
  };
  // clang-format on

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  for (size_t i = 2; i < std::size(data); ++i)
    matches[i].type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  EXPECT_EQ(4UL, result.size());
  EXPECT_NE(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
            result.match_at(0)->type);
  EXPECT_TRUE(result.match_at(0)->allowed_to_be_default_match);
  for (size_t i = 1; i < 4; ++i) {
    EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
              result.match_at(i)->type);
    EXPECT_FALSE(result.match_at(i)->allowed_to_be_default_match);
  }
}

TEST_F(AutocompleteResultTest, SortAndCullZeroRelevanceDefaultMatches) {
  // clang-format off
  TestData data[] = {
      {1, 1, 0,    true},   // A zero-relevance default non-tail suggestion.
      {2, 1, 1100, true},   // Default tail
      {3, 1, 1000, false},  // Tail
      {4, 1, 1300, false},  // Tail
      {5, 1, 0,    false},  // No-relevance tail suggestion.
  };
  // clang-format on

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  for (size_t i = 1; i < std::size(data); ++i)
    matches[i].type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  // It should ignore the first suggestion, despite it being marked as
  // allowed to be default.
  EXPECT_EQ(3UL, result.size());
  EXPECT_TRUE(result.match_at(0)->allowed_to_be_default_match);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
              result.match_at(i)->type);
    if (i > 0)
      EXPECT_FALSE(result.match_at(i)->allowed_to_be_default_match);
  }
}

#endif

TEST_F(AutocompleteResultTest, SortAndCullOnlyTailSuggestions) {
  // clang-format off
  TestData data[] = {
      {1, 1, 500,  true},   // Allow a bad non-tail default.
      {2, 1, 1100, false},  // Tail
      {3, 1, 1000, false},  // Tail
      {4, 1, 1300, false},  // Tail
      {5, 1, 1200, false},  // Tail
  };
  // clang-format on

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  // These will not be removed.
  for (size_t i = 1; i < 5; ++i)
    matches[i].type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  EXPECT_EQ(5UL, result.size());
  EXPECT_NE(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
            result.match_at(0)->type);
  for (size_t i = 1; i < 5; ++i)
    EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
              result.match_at(i)->type);
}

TEST_F(AutocompleteResultTest, SortAndCullNoMatchesAllowedToBeDefault) {
  // clang-format off
  TestData data[] = {
      {1, 1, 500,  false},  // Not allowed_to_be_default_match
      {2, 1, 1100, false},  // Not allowed_to_be_default_match
      {3, 1, 1000, false},  // Not allowed_to_be_default_match
  };
  // clang-format on

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);

  AutocompleteInput input(std::u16string(), metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  EXPECT_EQ(3UL, result.size());
  EXPECT_EQ(matches[1].destination_url, result.match_at(0)->destination_url);
  EXPECT_EQ(matches[2].destination_url, result.match_at(1)->destination_url);
  EXPECT_EQ(matches[0].destination_url, result.match_at(2)->destination_url);
  for (size_t i = 0; i < 3; ++i)
    EXPECT_FALSE(result.match_at(i)->allowed_to_be_default_match);
}

TEST_F(AutocompleteResultTest, SortAndCullDuplicateSearchURLs) {
  // Register a template URL that corresponds to 'foo' search engine.
  TemplateURLData url_data;
  url_data.SetShortName(u"unittest");
  url_data.SetKeyword(u"foo");
  url_data.SetURL("http://www.foo.com/s?q={searchTerms}");
  template_url_service().Add(std::make_unique<TemplateURL>(url_data));

  TestData data[] = {
      {0, 1, 1300, true}, {1, 1, 1200, true}, {2, 1, 1100, true},
      {3, 1, 1000, true}, {4, 2, 900, true},
  };

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  matches[0].destination_url = GURL("http://www.foo.com/s?q=foo");
  matches[1].destination_url = GURL("http://www.foo.com/s?q=foo2");
  matches[2].destination_url = GURL("http://www.foo.com/s?q=foo&oq=f");
  matches[3].destination_url = GURL("http://www.foo.com/s?q=foo&aqs=0");
  matches[4].destination_url = GURL("http://www.foo.com/");

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  // We expect the 3rd and 4th results to be removed.
  ASSERT_EQ(3U, result.size());
  EXPECT_EQ("http://www.foo.com/s?q=foo",
            result.match_at(0)->destination_url.spec());
  EXPECT_EQ(1300, result.match_at(0)->relevance);
  EXPECT_EQ("http://www.foo.com/s?q=foo2",
            result.match_at(1)->destination_url.spec());
  EXPECT_EQ(1200, result.match_at(1)->relevance);
  EXPECT_EQ("http://www.foo.com/", result.match_at(2)->destination_url.spec());
  EXPECT_EQ(900, result.match_at(2)->relevance);
}

TEST_F(AutocompleteResultTest, SortAndCullWithMatchDups) {
  // Register a template URL that corresponds to 'foo' search engine.
  TemplateURLData url_data;
  url_data.SetShortName(u"unittest");
  url_data.SetKeyword(u"foo");
  url_data.SetURL("http://www.foo.com/s?q={searchTerms}");
  template_url_service().Add(std::make_unique<TemplateURL>(url_data));

  AutocompleteMatch dup_match;
  dup_match.destination_url = GURL("http://www.foo.com/s?q=foo&oq=dup");
  std::vector<AutocompleteMatch> dups;
  dups.push_back(dup_match);

  TestData data[] = {
      {0, 1, 1300, true, dups}, {1, 1, 1200, true}, {2, 1, 1100, true},
      {3, 1, 1000, true, dups}, {4, 2, 900, true},  {5, 1, 800, true},
  };

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  matches[0].destination_url = GURL("http://www.foo.com/s?q=foo");
  matches[1].destination_url = GURL("http://www.foo.com/s?q=foo2");
  matches[2].destination_url = GURL("http://www.foo.com/s?q=foo&oq=f");
  matches[3].destination_url = GURL("http://www.foo.com/s?q=foo&aqs=0");
  matches[4].destination_url = GURL("http://www.foo.com/");
  matches[5].destination_url = GURL("http://www.foo.com/s?q=foo2&oq=f");

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  // Expect 3 unique results after SortAndCull().
  ASSERT_EQ(3U, result.size());

  // Check that 3rd and 4th result got added to the first result as duplicates
  // and also duplicates of the 4th match got copied.
  ASSERT_EQ(4U, result.match_at(0)->duplicate_matches.size());
  const AutocompleteMatch* first_match = result.match_at(0);
  EXPECT_EQ(matches[2].destination_url,
            first_match->duplicate_matches.at(1).destination_url);
  EXPECT_EQ(matches[3].destination_url,
            first_match->duplicate_matches.at(2).destination_url);
  EXPECT_EQ(dup_match.destination_url,
            first_match->duplicate_matches.at(3).destination_url);

  // Check that 6th result started a new list of dups for the second result.
  ASSERT_EQ(1U, result.match_at(1)->duplicate_matches.size());
  EXPECT_EQ(matches[5].destination_url,
            result.match_at(1)->duplicate_matches.at(0).destination_url);
}

TEST_F(AutocompleteResultTest, SortAndCullWithDemotionsByType) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kGroupingFrameworkForNonZPS);

  // Add some matches.
  ACMatches matches;
  const AutocompleteMatchTestData data[] = {
      {"http://history-url/", AutocompleteMatchType::HISTORY_URL},
      {"http://search-what-you-typed/",
       AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED},
      {"http://history-title/", AutocompleteMatchType::HISTORY_TITLE},
      {"http://search-history/", AutocompleteMatchType::SEARCH_HISTORY},
  };
  PopulateAutocompleteMatchesFromTestData(data, std::size(data), &matches);

  // Demote the search history match relevance score.
  matches.back().relevance = 500;

  // Add a rule demoting history-url and killing history-title.
  {
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kDemoteByTypeRule) + ":3:*"] =
        "1:50,7:100,2:0";  // 3 == HOME_PAGE
    ASSERT_TRUE(base::AssociateFieldTrialParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");

  AutocompleteInput input(u"a", OmniboxEventProto::HOME_PAGE,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  // Check the new ordering.  The history-title results should be omitted.
  // HistoryURL should still be first because type demotion is not applied to
  // the top match.
  size_t expected_order[] = {0, 1, 3};

  ASSERT_EQ(std::size(expected_order), result.size());
  for (size_t i = 0; i < std::size(expected_order); ++i) {
    EXPECT_EQ(data[expected_order[i]].destination_url,
              result.match_at(i)->destination_url.spec());
  }
}

TEST_F(AutocompleteResultTest, SortAndCullWithPreserveDefaultMatch) {
  auto test = [&](const std::vector<TestData>& last,
                  const std::vector<TestData>& current,
                  const std::vector<TestData>& expected) {
    AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());

    ACMatches last_matches = PopulateAutocompleteMatches(last);
    AutocompleteResult last_result;
    last_result.AppendMatches(last_matches);
    last_result.SortAndCull(input, &template_url_service(),
                            triggered_feature_service());

    ACMatches current_matches = PopulateAutocompleteMatches(current);
    AutocompleteResult current_result;
    current_result.AppendMatches(current_matches);

    // Run SortAndCull, but try to keep the first entry of last_matches on top.
    current_result.SortAndCull(input, &template_url_service(),
                               triggered_feature_service(),
                               *last_result.match_at(0));

    AssertResultMatches(current_result, expected);
  };

  {
    SCOPED_TRACE("Lower scored default is preserved.");
    std::vector<TestData> last = {
        {0, 1, 500, true},
        {1, 1, 400, true},
    };
    std::vector<TestData> current = {
        {1, 1, 500, true},
        {0, 1, 400, true},
    };
    std::vector<TestData> expected = {
        {0, 1, 400, true},
        {1, 1, 500, true},
    };
    test(last, current, expected);
  }
  {
    SCOPED_TRACE("Don't preserve a default that no longer matches.");
    std::vector<TestData> last = {
        {0, 1, 500, true},
    };
    std::vector<TestData> current = {
        {1, 1, 100, true},
    };
    std::vector<TestData> expected = {
        {1, 1, 100, true},
    };
    test(last, current, expected);
  }
  {
    SCOPED_TRACE(
        "Previous default does not replace a higher scored "
        "URL_WHAT_YOU_TYPED.");
    std::vector<TestData> last = {
        {0, 1, 500, true, {}, AutocompleteMatchType::HISTORY_URL},
        {1, 1, 400, true, {}, AutocompleteMatchType::HISTORY_URL},
    };
    std::vector<TestData> current = {
        {0, 1, 500, true, {}, AutocompleteMatchType::HISTORY_URL},
        {1, 1, 400, true, {}, AutocompleteMatchType::HISTORY_URL},
        {2, 1, 600, true, {}, AutocompleteMatchType::URL_WHAT_YOU_TYPED},
    };
    std::vector<TestData> expected = {
        {2, 1, 600, true, {}, AutocompleteMatchType::URL_WHAT_YOU_TYPED},
        {0, 1, 500, true, {}, AutocompleteMatchType::HISTORY_URL},
        {1, 1, 400, true, {}, AutocompleteMatchType::HISTORY_URL},
    };
    test(last, current, expected);
  }
  {
    SCOPED_TRACE(
        "Previous default does replace a lower scored URL_WHAT_YOU_TYPED.");
    std::vector<TestData> last = {
        {0, 1, 500, true, {}, AutocompleteMatchType::HISTORY_URL},
        {1, 1, 400, true, {}, AutocompleteMatchType::HISTORY_URL},
    };
    std::vector<TestData> current = {
        {0, 1, 500, true, {}, AutocompleteMatchType::HISTORY_URL},
        {1, 1, 400, true, {}, AutocompleteMatchType::HISTORY_URL},
        {2, 1, 300, true, {}, AutocompleteMatchType::URL_WHAT_YOU_TYPED},
        {3, 1, 600, true, {}, AutocompleteMatchType::HISTORY_URL},
    };
    std::vector<TestData> expected = {
        {0, 1, 500, true, {}, AutocompleteMatchType::HISTORY_URL},
        {3, 1, 600, true, {}, AutocompleteMatchType::HISTORY_URL},
        {1, 1, 400, true, {}, AutocompleteMatchType::HISTORY_URL},
        {2, 1, 300, true, {}, AutocompleteMatchType::URL_WHAT_YOU_TYPED},
    };
    test(last, current, expected);
  }
}

TEST_F(AutocompleteResultTest, DemoteOnDeviceSearchSuggestions) {
  // clang-format off
  TestData data[] = {
      {1, 1, 500,  true},
      {2, 2, 1100, true},
      {3, 2, 1000, true},
      {4, 1, 1300, true},
      {5, 1, 1200, true},
  };
  // clang-format on

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  matches[0].type = AutocompleteMatchType::SEARCH_SUGGEST;
  matches[1].type = AutocompleteMatchType::SEARCH_SUGGEST;
  matches[2].type = AutocompleteMatchType::SEARCH_SUGGEST;
  matches[3].type = AutocompleteMatchType::SEARCH_SUGGEST;
  matches[4].type = AutocompleteMatchType::SEARCH_SUGGEST;

  // match1, match2 are set as on device head suggestion.
  matches[1].subtypes = {omnibox::SUBTYPE_OMNIBOX_OTHER,
                         omnibox::SUBTYPE_SUGGEST_2G_LITE};
  matches[2].subtypes = {omnibox::SUBTYPE_OMNIBOX_OTHER,
                         omnibox::SUBTYPE_SUGGEST_2G_LITE};
  matches[0].provider->type_ = AutocompleteProvider::TYPE_SEARCH;
  matches[1].provider->type_ = AutocompleteProvider::TYPE_ON_DEVICE_HEAD;

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  // Test setting on device suggestion relevances lower than search provider
  // suggestions.
  AutocompleteResultForTesting result;
  result.AppendMatches(matches);
  result.DemoteOnDeviceSearchSuggestions();
  EXPECT_EQ(5UL, result.size());
  EXPECT_NE(AutocompleteProvider::TYPE_ON_DEVICE_HEAD,
            result.match_at(0)->provider->type());
  EXPECT_EQ(AutocompleteProvider::TYPE_ON_DEVICE_HEAD,
            result.match_at(1)->provider->type());
  EXPECT_LT(result.match_at(1)->relevance, result.match_at(0)->relevance);
  EXPECT_EQ(AutocompleteProvider::TYPE_ON_DEVICE_HEAD,
            result.match_at(2)->provider->type());
  EXPECT_LT(result.match_at(2)->relevance, result.match_at(0)->relevance);
  EXPECT_NE(AutocompleteProvider::TYPE_ON_DEVICE_HEAD,
            result.match_at(3)->provider->type());
  EXPECT_NE(AutocompleteProvider::TYPE_ON_DEVICE_HEAD,
            result.match_at(4)->provider->type());
}

TEST_F(AutocompleteResultTest, DemoteByType) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kGroupingFrameworkForNonZPS);

  // Add some matches.
  ACMatches matches;
  const AutocompleteMatchTestData data[] = {
      {"http://history-url/", AutocompleteMatchType::HISTORY_URL},
      {"http://history-title/", AutocompleteMatchType::HISTORY_TITLE},
      {"http://search-what-you-typed/",
       AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED},
      {"http://search-history/", AutocompleteMatchType::SEARCH_HISTORY},
  };
  PopulateAutocompleteMatchesFromTestData(data, std::size(data), &matches);

  // Make history-title and search-history the only default matches, so that
  // they compete.
  matches[0].allowed_to_be_default_match = false;
  matches[2].allowed_to_be_default_match = false;

  // Add a rule demoting history-title.
  {
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kDemoteByTypeRule) + ":*:*"] = "2:50";
    ASSERT_TRUE(base::AssociateFieldTrialParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Where Grouping suggestions by Search vs URL kicks in, search gets
  // promoted to the top of the list.
  const std::vector<size_t> expected_natural_order{1, 2, 3, 0};
  const std::vector<size_t> expected_demoted_order{3, 2, 0, 1};
#elif BUILDFLAG(IS_IOS)
  // Temporary while adaptive suggestion is still in experimentation on iOS.
  std::vector<size_t> expected_natural_order;
  std::vector<size_t> expected_demoted_order;
  expected_natural_order = std::vector<size_t>{1, 0, 2, 3};
  expected_demoted_order = std::vector<size_t>{3, 0, 2, 1};
#else
  // Note: Android and iOS performs grouping by Search vs URL at a later stage,
  // when views are built. this means the vector below will be demoted by type,
  // but not rearranged by Search vs URL.
  const std::vector<size_t> expected_natural_order{1, 0, 2, 3};
  const std::vector<size_t> expected_demoted_order{3, 0, 2, 1};
#endif

  // Because we want to ensure the highest naturally scoring
  // allowed-to-be default suggestion is the default, make sure history-title
  // is the default match despite demotion.
  // Make sure history-URL is the last match due to the logic which groups
  // searches and URLs together.
  SortMatchesAndVerifyOrder("a", OmniboxEventProto::HOME_PAGE, matches,
                            expected_natural_order, data);

  // However, in the fakebox/realbox, we do want to use the demoted score when
  // selecting the default match because we generally only expect it to be
  // used for queries and we demote URLs strongly. So here we re-sort with a
  // page classification of fakebox/realbox, and make sure history-title is now
  // demoted. We also make sure history-URL is the last match due to the logic
  // which groups searches and URLs together.
  SortMatchesAndVerifyOrder("a", OmniboxEventProto::NTP_REALBOX, matches,
                            expected_demoted_order, data);
  SortMatchesAndVerifyOrder("a", OmniboxEventProto::NTP_REALBOX, matches,
                            expected_demoted_order, data);

  // Unless, the user's input looks like a URL, in which case we want to use
  // the natural scoring again to make sure the user gets a URL if they're
  // clearly trying to navigate. So here we re-sort with a page classification
  // of fakebox/realbox and an input that's a URL, and make sure history-title
  // is once again the default match.
  SortMatchesAndVerifyOrder("www.example.com", OmniboxEventProto::NTP_REALBOX,
                            matches, expected_natural_order, data);
  SortMatchesAndVerifyOrder("www.example.com", OmniboxEventProto::NTP_REALBOX,
                            matches, expected_natural_order, data);
}

TEST_F(AutocompleteResultTest, SortAndCullReorderForDefaultMatch) {
  TestData data[] = {{0, 1, 1300, true},
                     {1, 1, 1200, true},
                     {2, 1, 1100, true},
                     {3, 1, 1000, true}};
  TestSchemeClassifier test_scheme_classifier;

  {
    // Check that reorder doesn't do anything if the top result
    // is already a legal default match (which is the default from
    // PopulateAutocompleteMatches()).
    ACMatches matches;
    PopulateAutocompleteMatches(data, std::size(data), &matches);
    AutocompleteInput input(u"a", metrics::OmniboxEventProto::HOME_PAGE,
                            test_scheme_classifier);
    AutocompleteResult result;
    result.AppendMatches(matches);
    result.SortAndCull(input, &template_url_service(),
                       triggered_feature_service());
    AssertResultMatches(result, data);
  }

  {
    // Check that reorder swaps up a result appropriately.
    ACMatches matches;
    PopulateAutocompleteMatches(data, std::size(data), &matches);
    matches[0].allowed_to_be_default_match = false;
    matches[1].allowed_to_be_default_match = false;
    AutocompleteInput input(u"a", metrics::OmniboxEventProto::HOME_PAGE,
                            test_scheme_classifier);
    AutocompleteResult result;
    result.AppendMatches(matches);
    result.SortAndCull(input, &template_url_service(),
                       triggered_feature_service());
    ASSERT_EQ(4U, result.size());
    EXPECT_EQ("http://c/", result.match_at(0)->destination_url.spec());
    EXPECT_EQ("http://a/", result.match_at(1)->destination_url.spec());
    EXPECT_EQ("http://b/", result.match_at(2)->destination_url.spec());
    EXPECT_EQ("http://d/", result.match_at(3)->destination_url.spec());
  }
}

// Note: DCHECKs not firing on Cast.
#if DCHECK_IS_ON()
TEST_F(AutocompleteResultTest, SortAndCullFailsWithIncorrectDefaultScheme) {
  // Scenario:
  // - User navigates to a webpage whose URL looks like a scheme,
  //   e.g. "chrome:123" (note the colon).
  // - The navigation becomes an entry in the user's URL history.
  // - User types "chrome:" again.
  // - The top suggestion will match the previous user entry and may be the
  //   "chrome:123" again, except typing ":" invokes scheme checker.
  // Make sure the scheme checker is not causing trouble when the default
  // suggestion is Search.

  const AutocompleteMatchTestData data[] = {
      {"https://chrome:123", AutocompleteMatchType::HISTORY_URL},
      {"chrome://history", AutocompleteMatchType::HISTORY_URL}};
  ACMatches matches;
  PopulateAutocompleteMatchesFromTestData(data, std::size(data), &matches);
  matches[0].allowed_to_be_default_match = true;
  matches[1].allowed_to_be_default_match = true;
  TestSchemeClassifier test_scheme_classifier;

  AutocompleteInput input(u"chrome:", metrics::OmniboxEventProto::HOME_PAGE,
                          test_scheme_classifier);
  AutocompleteResult result;
  result.AppendMatches(matches);
  EXPECT_DEATH_IF_SUPPORTED(result.SortAndCull(input, &template_url_service(),
                                               triggered_feature_service()),
                            "");
}
#endif

TEST_F(AutocompleteResultTest, SortAndCullPermitSearchForSchemeMatching) {
  // Scenario:
  // - User searches for something that looks like a scheme,
  //   e.g. "chrome: how to do x" (note the colon).
  // - The search becomes an entry in the user's history (local or remote)
  //   outranking any HistoryURL matches.
  // - User types "chrome:" again.
  // - The top suggestion will match the previous user entry and may be the
  //   "chrome: how to do x" again, except typing ":" invokes scheme checker.
  // Make sure the scheme checker is not causing trouble when the default
  // suggestion is Search.
  const AutocompleteMatchTestData data[] = {
      {"https://google.com/search?q=chrome:123",
       AutocompleteMatchType::SEARCH_SUGGEST},
      {"chrome://history", AutocompleteMatchType::HISTORY_URL}};
  ACMatches matches;
  PopulateAutocompleteMatchesFromTestData(data, std::size(data), &matches);
  matches[0].allowed_to_be_default_match = true;
  matches[1].allowed_to_be_default_match = true;
  TestSchemeClassifier test_scheme_classifier;

  AutocompleteInput input(u"chrome:", metrics::OmniboxEventProto::HOME_PAGE,
                          test_scheme_classifier);
  AutocompleteResult result;
  result.AppendMatches(matches);
  // Must not assert.
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());
}

TEST_F(AutocompleteResultTest, SortAndCullPromoteDefaultMatch) {
  TestData data[] = {{0, 1, 1300, false},
                     {1, 1, 1200, false},
                     {2, 2, 1100, false},
                     {2, 3, 1000, false},
                     {2, 4, 900, true}};
  TestSchemeClassifier test_scheme_classifier;

  // Check that reorder swaps up a result, and promotes relevance,
  // appropriately.
  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  AutocompleteInput input(u"a", metrics::OmniboxEventProto::HOME_PAGE,
                          test_scheme_classifier);
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());
  ASSERT_EQ(3U, result.size());
  EXPECT_EQ("http://c/", result.match_at(0)->destination_url.spec());
  EXPECT_EQ(1100, result.match_at(0)->relevance);
  EXPECT_TRUE(result.match_at(0)->allowed_to_be_default_match);
  EXPECT_EQ(GetProvider(4), result.match_at(0)->provider);
  EXPECT_EQ("http://a/", result.match_at(1)->destination_url.spec());
  EXPECT_FALSE(result.match_at(1)->allowed_to_be_default_match);
  EXPECT_EQ("http://b/", result.match_at(2)->destination_url.spec());
  EXPECT_FALSE(result.match_at(2)->allowed_to_be_default_match);
}

TEST_F(AutocompleteResultTest, SortAndCullPromoteUnconsecutiveMatches) {
  TestData data[] = {
      {0, 1, 1300, false}, {1, 1, 1200, true}, {3, 2, 1100, false},
      {2, 1, 1000, false}, {3, 3, 900, true},  {4, 1, 800, false},
      {3, 4, 700, false},
  };
  TestSchemeClassifier test_scheme_classifier;

  // Check that reorder swaps up a result, and promotes relevance,
  // even for a default match that isn't the best.
  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  AutocompleteInput input(u"a", metrics::OmniboxEventProto::HOME_PAGE,
                          test_scheme_classifier);
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());
  ASSERT_EQ(5U, result.size());
  EXPECT_EQ("http://b/", result.match_at(0)->destination_url.spec());
  EXPECT_EQ(1200, result.match_at(0)->relevance);
  EXPECT_EQ("http://a/", result.match_at(1)->destination_url.spec());
  EXPECT_EQ("http://d/", result.match_at(2)->destination_url.spec());
  EXPECT_EQ(1100, result.match_at(2)->relevance);
  EXPECT_EQ(GetProvider(3), result.match_at(2)->provider);
  EXPECT_EQ("http://c/", result.match_at(3)->destination_url.spec());
  EXPECT_EQ("http://e/", result.match_at(4)->destination_url.spec());
}

struct EntityTestData {
  AutocompleteMatchType::Type type;
  raw_ptr<FakeAutocompleteProvider> provider;
  std::string destination_url;
  int relevance;
  bool allowed_to_be_default_match;
  std::string fill_into_edit;
  std::string inline_autocompletion;
};

void PopulateEntityTestCases(std::vector<EntityTestData>& test_cases,
                             ACMatches* matches) {
  for (const auto& test_case : test_cases) {
    AutocompleteMatch match;
    match.provider = test_case.provider;
    match.type = test_case.type;
    match.destination_url = GURL(test_case.destination_url);
    match.relevance = test_case.relevance;
    match.allowed_to_be_default_match = test_case.allowed_to_be_default_match;
    match.fill_into_edit = base::UTF8ToUTF16(test_case.fill_into_edit);
    match.inline_autocompletion =
        base::UTF8ToUTF16(test_case.inline_autocompletion);
    matches->push_back(match);
  }
}

TEST_F(AutocompleteResultTest, SortAndCullPreferEntities) {
  // clang-format off
  std::vector<EntityTestData> test_cases = {
    {
      AutocompleteMatchType::SEARCH_SUGGEST, GetProvider(1),
      "http://search/?q=foo", 1100, false, "foo", ""
    },
    {
      AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, GetProvider(1),
      "http://search/?q=foo", 1000, false, "foo", ""
    },
    {
      AutocompleteMatchType::SEARCH_SUGGEST, GetProvider(1),
      "http://search/?q=foo", 900, true, "foo", "oo"
    },
    // This match will be the first result but it won't affect the entity
    // deduping because it has a different URL.
    //
    // Also keeping this as the default match allows us to test that Entities
    // and plain matches are deduplicated when they are not the default match.
    // See SortAndCullPreferEntitiesButKeepDefaultPlainMatches for details.
    {
      AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED, GetProvider(1),
      "http://search/?q=bar", 1200, true, "foo", "oo"
    },
  };
  // clang-format on
  ACMatches matches;
  PopulateEntityTestCases(test_cases, &matches);

  AutocompleteInput input(u"f", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  // The first result will be the personalized suggestion.
  EXPECT_EQ(2UL, result.size());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED,
            result.match_at(0)->type);
  EXPECT_EQ(1200, result.match_at(0)->relevance);

  // The second result will be the result of deduping the other three.
  // The chosen match should be the entity suggestion and it should have been
  // promoted to receive the first match's relevance and the last match's
  // allowed_to_be_default_match and inline_autocompletion values.
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
            result.match_at(1)->type);
  EXPECT_EQ(1100, result.match_at(1)->relevance);
  EXPECT_TRUE(result.match_at(1)->allowed_to_be_default_match);
  EXPECT_EQ(u"oo", result.match_at(1)->inline_autocompletion);
}

TEST_F(AutocompleteResultTest,
       SortAndCullPreferNonEntitiesForDefaultSuggestion) {
  // When the top scoring allowed_to_be_default suggestion is a search entity,
  // and there is a duplicate non-entity search suggest that is also
  // allowed_to_be_default, prefer the latter.

  std::vector<EntityTestData> test_cases = {
      {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, GetProvider(1),
       "http://search/?q=foo", 1000, true},
      {AutocompleteMatchType::SEARCH_SUGGEST, GetProvider(1),
       "http://search/?q=foo2", 1200},
      // A duplicate search suggestion should be preferred to a search entity
      // suggestion, even if the former is scored lower.
      {AutocompleteMatchType::SEARCH_SUGGEST, GetProvider(1),
       "http://search/?q=foo", 900, true},
  };
  ACMatches matches;
  PopulateEntityTestCases(test_cases, &matches);

  // Simulate the search provider pre-grouping duplicate suggestions. We want
  // to make sure `stripped_destination_url` is correctly appended to pre-duped
  // suggestions.
  matches[0].duplicate_matches.push_back(matches.back());
  matches.pop_back();

  // Simulate the top match having an `entity_id`.
  matches[0].entity_id = "/m/012abc";

  AutocompleteInput input(u"f", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  ASSERT_EQ(result.size(), 3u);

  auto* match = result.match_at(0);
  EXPECT_EQ(match->type, AutocompleteMatchType::SEARCH_SUGGEST);
  // Should not inherit the search entity suggestion's relevance; only the
  // non-dup suggestion inherits from the dup suggestions; not vice versa.
  EXPECT_EQ(match->relevance, 900);
  EXPECT_TRUE(match->allowed_to_be_default_match);
  EXPECT_EQ(match->stripped_destination_url.spec(), "http://search/?q=foo");
  // Expect that the Entity's ID was merged over to the plain match equivalent.
  EXPECT_EQ(match->entity_id, "/m/012abc");

  match = result.match_at(1);
  // The search entity suggestion should be ranked higher than the higher
  // scoring 'foo2' search suggestion. When demoting default entity suggestions,
  // they are moved to position 2 rather than re-ranked according to their
  // relevance.
  EXPECT_EQ(match->type, AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);
  EXPECT_EQ(match->relevance, 1000);
  EXPECT_TRUE(match->allowed_to_be_default_match);
  EXPECT_EQ(match->stripped_destination_url.spec(), "http://search/?q=foo");
  EXPECT_EQ(match->entity_id, "/m/012abc");

  match = result.match_at(2);
  EXPECT_EQ(match->type, AutocompleteMatchType::SEARCH_SUGGEST);
  EXPECT_EQ(match->relevance, 1200);
  EXPECT_FALSE(match->allowed_to_be_default_match);
  EXPECT_EQ(match->stripped_destination_url.spec(), "http://search/?q=foo2");
  EXPECT_TRUE(match->entity_id.empty());
}

TEST_F(AutocompleteResultTest,
       SortAndCullDontPreferNonEntityNonDefaultForDefaultSuggestion) {
  // When the top scoring allowed_to_be_default suggestion is a search entity,
  // and there are no duplicate allowed_to_be_default suggestions, keep the
  // search entity suggestion default.

  std::vector<EntityTestData> test_cases = {
      {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, GetProvider(1),
       "http://search/?q=foo", 1000, true},
      // A duplicate non-allowed_to_be_default search suggestion should not be
      // preferred to a lower ranked search entity suggestion.
      {AutocompleteMatchType::SEARCH_SUGGEST, GetProvider(1),
       "http://search/?q=foo", 1300},
      // A non-duplicate allowed_to_be_default search suggestion should not be
      // preferred to a higher ranked search entity suggestion.
      {AutocompleteMatchType::SEARCH_SUGGEST, GetProvider(1),
       "http://search/?q=foo2", 900, true},
  };
  ACMatches matches;
  PopulateEntityTestCases(test_cases, &matches);

  AutocompleteInput input(u"f", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  ASSERT_EQ(result.size(), 2u);

  auto* match = result.match_at(0);
  EXPECT_EQ(match->type, AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);
  EXPECT_EQ(match->relevance, 1300);
  EXPECT_TRUE(match->allowed_to_be_default_match);

  match = result.match_at(1);
  EXPECT_EQ(match->type, AutocompleteMatchType::SEARCH_SUGGEST);
  EXPECT_EQ(match->relevance, 900);
  EXPECT_TRUE(match->allowed_to_be_default_match);
}

TEST_F(AutocompleteResultTest, SortAndCullPreferEntitiesFillIntoEditMustMatch) {
  // clang-format off
  std::vector<EntityTestData> test_cases = {
    {
      AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED, GetProvider(1),
      "http://search/?q=foo", 1100, false, "foo", ""
    },
    {
      AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, GetProvider(1),
      "http://search/?q=foo", 1000, false, "foobar", ""
    },
    {
      AutocompleteMatchType::SEARCH_SUGGEST, GetProvider(1),
      "http://search/?q=foo", 900, true, "foo", "oo"
    },
  };
  // clang-format on
  ACMatches matches;
  PopulateEntityTestCases(test_cases, &matches);

  AutocompleteInput input(u"f", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  // The entity suggestion won't be chosen in this case because it has a non-
  // matching value for fill_into_edit.
  EXPECT_EQ(1UL, result.size());
  // But the final type will have the specialized Search History type, since
  // that's consumed into the final match during the merge step.
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED,
            result.match_at(0)->type);
  EXPECT_EQ(1100, result.match_at(0)->relevance);
  EXPECT_TRUE(result.match_at(0)->allowed_to_be_default_match);
  EXPECT_EQ(u"oo", result.match_at(0)->inline_autocompletion);
}

TEST_F(AutocompleteResultTest,
       SortAndCullPreferEntitiesButKeepDefaultPlainMatches) {
  // clang-format off
  std::vector<EntityTestData> test_cases = {
    {
      AutocompleteMatchType::SEARCH_SUGGEST, GetProvider(1),
      "http://search/?q=foo", 1001, true, "foo", ""
    },
    {
      AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, GetProvider(1),
      "http://search/?q=foo", 1000, false, "foo", ""
    },
    {
      AutocompleteMatchType::SEARCH_SUGGEST, GetProvider(1),
      "http://search/?q=foo", 900, true, "foo", "oo"
    },
  };
  // clang-format on
  ACMatches matches;
  PopulateEntityTestCases(test_cases, &matches);

  AutocompleteInput input(u"f", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  // The first result will be a plain match.
  EXPECT_EQ(2UL, result.size());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST, result.match_at(0)->type);
  EXPECT_EQ(1001, result.match_at(0)->relevance);

  // The second result will be the result of deduping the Suggest Entity with
  // the third result. It should have still consumed the inline autocomplete
  // and allowed_to_be_default qualities from the other two.
  EXPECT_EQ(AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
            result.match_at(1)->type);
  EXPECT_EQ(1001, result.match_at(1)->relevance);
  EXPECT_TRUE(result.match_at(1)->allowed_to_be_default_match);
  EXPECT_EQ(u"oo", result.match_at(1)->inline_autocompletion);
}

TEST_F(
    AutocompleteResultTest,
    SortAndCullPreferNonEntitySpecializedSearchSuggestionForDefaultSuggestion) {
  // When selecting among multiple duplicate non-entity suggestions, prefer
  // promoting the one that is a SEARCH_SUGGEST or other "specialized" search
  // suggestion.

  std::vector<EntityTestData> test_cases = {
      // Entity search suggestion.
      {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, GetProvider(1),
       "http://search/?q=foo", 1200, true},
      // Duplicate non-entity SEARCH_SUGGEST suggestion should be preferred
      // above the other options (even when scoring lower).
      {AutocompleteMatchType::SEARCH_SUGGEST, GetProvider(1),
       "http://search/?q=foo", 1000, true},
      // Duplicate non-entity match which is neither a SEARCH_SUGGEST nor a
      // "specialized" search suggestion.
      {AutocompleteMatchType::SEARCH_HISTORY, GetProvider(1),
       "http://search/?q=foo", 1100, true},
  };
  ACMatches matches;
  PopulateEntityTestCases(test_cases, &matches);

  // Simulate the search provider pre-grouping duplicate suggestions.
  matches[0].duplicate_matches.push_back(matches.back());
  matches.pop_back();

  matches[0].duplicate_matches.push_back(matches.back());
  matches.pop_back();

  AutocompleteInput input(u"f", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  ASSERT_EQ(result.size(), 2u);

  auto* match = result.match_at(0);
  // The non-entity SEARCH_SUGGEST suggestion should be promoted as the top
  // match.
  EXPECT_EQ(match->type, AutocompleteMatchType::SEARCH_SUGGEST);
  EXPECT_TRUE(match->allowed_to_be_default_match);

  match = result.match_at(1);
  // The entity search suggestion should have been demoted.
  EXPECT_EQ(match->type, AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);
  EXPECT_TRUE(match->allowed_to_be_default_match);

  EXPECT_EQ(match->duplicate_matches.size(), 1u);

  // The non-entity match which is neither a SEARCH_SUGGEST nor a "specialized"
  // search suggestion should remain in |duplicate_matches| (i.e. it's not
  // promoted as the top match).
  EXPECT_EQ(match->duplicate_matches.at(0).type,
            AutocompleteMatchType::SEARCH_HISTORY);
  EXPECT_TRUE(match->duplicate_matches.at(0).allowed_to_be_default_match);
}

TEST_F(AutocompleteResultTest, SortAndCullPromoteDuplicateSearchURLs) {
  // Register a template URL that corresponds to 'foo' search engine.
  TemplateURLData url_data;
  url_data.SetShortName(u"unittest");
  url_data.SetKeyword(u"foo");
  url_data.SetURL("http://www.foo.com/s?q={searchTerms}");
  template_url_service().Add(std::make_unique<TemplateURL>(url_data));

  TestData data[] = {
      {0, 1, 1300, false}, {1, 1, 1200, true}, {2, 1, 1100, true},
      {3, 1, 1000, true},  {4, 2, 900, true},
  };

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  // Note that 0, 2 and 3 will compare equal after stripping.
  matches[0].destination_url = GURL("http://www.foo.com/s?q=foo");
  matches[1].destination_url = GURL("http://www.foo.com/s?q=foo2");
  matches[2].destination_url = GURL("http://www.foo.com/s?q=foo&oq=f");
  matches[3].destination_url = GURL("http://www.foo.com/s?q=foo&aqs=0");
  matches[4].destination_url = GURL("http://www.foo.com/");

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  // We expect the 3rd and 4th results to be removed.
  ASSERT_EQ(3U, result.size());
  EXPECT_EQ("http://www.foo.com/s?q=foo&oq=f",
            result.match_at(0)->destination_url.spec());
  EXPECT_EQ(1300, result.match_at(0)->relevance);
  EXPECT_EQ("http://www.foo.com/s?q=foo2",
            result.match_at(1)->destination_url.spec());
  EXPECT_EQ(1200, result.match_at(1)->relevance);
  EXPECT_EQ("http://www.foo.com/", result.match_at(2)->destination_url.spec());
  EXPECT_EQ(900, result.match_at(2)->relevance);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutocompleteResultTest, SortAndCullFeaturedSearchBeforeStarterPack) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kUIExperimentMaxAutocompleteMatches,
      {{OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "5"}});
  TestData data[] = {
      {1, 1, 500, false, {}, AutocompleteMatchType::STARTER_PACK},
      {2, 1, 500, false, {}, AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH},
      {3, 1, 500, false, {}, AutocompleteMatchType::STARTER_PACK},
      {4, 1, 500, false, {}, AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH},
      {5, 2, 900, true, {}, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED},
  };
  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);

  AutocompleteInput input(u"@", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  ASSERT_EQ(5U, AutocompleteResult::GetMaxMatches(/*is_zero_suggest=*/false));
  const std::array<TestData, 5> expected_data{{
      // Default match ranks higher.
      {5, 2, 900, true, {}, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED},
      // Featured enterprise search.
      {2, 1, 500, false, {}, AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH},
      {4, 1, 500, false, {}, AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH},
      // Starter pack after featured search.
      {1, 1, 500, false, {}, AutocompleteMatchType::STARTER_PACK},
      {3, 1, 500, false, {}, AutocompleteMatchType::STARTER_PACK},
  }};
  AssertResultMatches(result, expected_data);
}
#endif

TEST_F(AutocompleteResultTest,
       GroupSuggestionsBySearchVsURLHonorsProtectedSuggestions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kUIExperimentMaxAutocompleteMatches,
      {{OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "7"}});
  TestData data[] = {
      {0, 2, 400, true, {}, AutocompleteMatchType::HISTORY_TITLE},
      {1, 1, 800, false, {}, AutocompleteMatchType::CLIPBOARD_URL},
      {2, 1, 700, false, {}, AutocompleteMatchType::TILE_NAVSUGGEST},
      {3, 1, 600, false, {}, AutocompleteMatchType::TILE_SUGGESTION},
      {4, 1, 1000, false, {}, AutocompleteMatchType::HISTORY_URL},
      {5, 1, 900, false, {}, AutocompleteMatchType::SEARCH_SUGGEST},
      {6, 1, 800, false, {}, AutocompleteMatchType::SEARCH_SUGGEST},
  };

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResultForTesting result;
  result.AppendMatches(matches);
  result.GroupSuggestionsBySearchVsURL(std::next(result.matches_.begin()),
                                       result.matches_.end());

  TestData expected_data[] = {
      {0, 2, 400, true, {}, AutocompleteMatchType::HISTORY_TITLE},
      {1, 1, 800, false, {}, AutocompleteMatchType::CLIPBOARD_URL},
      {2, 1, 700, false, {}, AutocompleteMatchType::TILE_NAVSUGGEST},
      {3, 1, 600, false, {}, AutocompleteMatchType::TILE_SUGGESTION},
      {5, 1, 900, false, {}, AutocompleteMatchType::SEARCH_SUGGEST},
      {6, 1, 800, false, {}, AutocompleteMatchType::SEARCH_SUGGEST},
      {4, 1, 1000, false, {}, AutocompleteMatchType::HISTORY_URL},
  };

  AssertResultMatches(
      result,
      {expected_data, expected_data + AutocompleteResult::GetMaxMatches()});
}

TEST_F(AutocompleteResultTest, SortAndCullMaxHistoryClusterSuggestions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kGroupingFrameworkForNonZPS);

  // Should limit history cluster suggestions to 1, even if there are no
  // alternative suggestions to display.

  ACMatches matches;
  const AutocompleteMatchTestData data[] = {
      {"url_1", AutocompleteMatchType::HISTORY_CLUSTER},
      {"url_2", AutocompleteMatchType::HISTORY_CLUSTER},
      {"url_3", AutocompleteMatchType::HISTORY_CLUSTER},
  };
  PopulateAutocompleteMatchesFromTestData(data, std::size(data), &matches);
  for (auto& m : matches)
    m.allowed_to_be_default_match = false;

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.match_at(0)->type, AutocompleteMatchType::HISTORY_CLUSTER);
}

TEST_F(AutocompleteResultTest, SortAndCullMaxURLMatches) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{omnibox::kUIExperimentMaxAutocompleteMatches,
        {{OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}}},
       {omnibox::kOmniboxMaxURLMatches,
        {{OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, "3"}}}},
      {omnibox::kGroupingFrameworkForNonZPS});

  EXPECT_TRUE(OmniboxFieldTrial::IsMaxURLMatchesFeatureEnabled());
  EXPECT_EQ(OmniboxFieldTrial::GetMaxURLMatches(), 3u);

  // Case 1: Eject URL match for a search.
  // Does not apply to Android and iOS which picks top N matches and performs
  // group by search vs URL separately (Adaptive Suggestions).
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  {
    ACMatches matches;
    const AutocompleteMatchTestData data[] = {
        {"http://search-what-you-typed/",
         AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED},
        {"http://search-history/", AutocompleteMatchType::SEARCH_HISTORY},
        {"http://history-url/", AutocompleteMatchType::HISTORY_URL},
        {"http://history-title/", AutocompleteMatchType::HISTORY_TITLE},
        {"http://url-what-you-typed/",
         AutocompleteMatchType::URL_WHAT_YOU_TYPED},
        {"http://clipboard-url/", AutocompleteMatchType::CLIPBOARD_URL},
        {"http://search-suggest/", AutocompleteMatchType::SEARCH_SUGGEST},
    };
    PopulateAutocompleteMatchesFromTestData(data, std::size(data), &matches);

    AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    AutocompleteResult result;
    result.AppendMatches(matches);
    result.SortAndCull(input, &template_url_service(),
                       triggered_feature_service());

    // Expect the search suggest to be moved about URL suggestions due to
    // the logic which groups searches and URLs together.
    AutocompleteMatchType::Type expected_types[] = {
        AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
        AutocompleteMatchType::SEARCH_HISTORY,
        AutocompleteMatchType::SEARCH_SUGGEST,
        AutocompleteMatchType::HISTORY_URL,
        AutocompleteMatchType::HISTORY_TITLE,
        AutocompleteMatchType::URL_WHAT_YOU_TYPED,
    };
    EXPECT_EQ(result.size(), AutocompleteResult::GetMaxMatches());
    for (size_t i = 0; i < result.size(); ++i)
      EXPECT_EQ(result.match_at(i)->type, expected_types[i]);
  }
#endif

  // Case 2: Do not eject URL match because there's no replacement.
  {
    ACMatches matches;
    const AutocompleteMatchTestData data[] = {
        {"http://search-what-you-typed/",
         AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED},
        {"http://search-history/", AutocompleteMatchType::SEARCH_HISTORY},
        {"http://history-url/", AutocompleteMatchType::HISTORY_URL},
        {"http://history-title/", AutocompleteMatchType::HISTORY_TITLE},
        {"http://url-what-you-typed/",
         AutocompleteMatchType::URL_WHAT_YOU_TYPED},
        {"http://clipboard-url/", AutocompleteMatchType::CLIPBOARD_URL},
        {"http://bookmark-title/", AutocompleteMatchType::BOOKMARK_TITLE},
    };
    PopulateAutocompleteMatchesFromTestData(data, std::size(data), &matches);

    AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    AutocompleteResult result;
    result.AppendMatches(matches);
    result.SortAndCull(input, &template_url_service(),
                       triggered_feature_service());

    EXPECT_EQ(result.size(), AutocompleteResult::GetMaxMatches());
    AutocompleteMatchType::Type expected_types[] = {
        AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
        AutocompleteMatchType::SEARCH_HISTORY,
        AutocompleteMatchType::HISTORY_URL,
        AutocompleteMatchType::HISTORY_TITLE,
        AutocompleteMatchType::URL_WHAT_YOU_TYPED,
        AutocompleteMatchType::CLIPBOARD_URL,
    };
    for (size_t i = 0; i < result.size(); ++i)
      EXPECT_EQ(result.match_at(i)->type, expected_types[i]) << i;
  }
}

TEST_F(AutocompleteResultTest, ConvertsOpenTabsCorrectly) {
  AutocompleteResult result;
  ACMatches matches;
  AutocompleteMatch match;
  match.destination_url = GURL("http://this-site-matches.com");
  matches.push_back(match);
  match.destination_url = GURL("http://other-site-matches.com");
  match.description = u"Some Other Site";
  matches.push_back(match);
  match.destination_url = GURL("http://doesnt-match.com");
  match.description = std::u16string();
  matches.push_back(match);
  result.AppendMatches(matches);

  // Have IsTabOpenWithURL() return true for some URLs.
  FakeAutocompleteProviderClient client;
  static_cast<FakeTabMatcher&>(const_cast<TabMatcher&>(client.GetTabMatcher()))
      .set_url_substring_match("matches");

  result.ConvertOpenTabMatches(&client, nullptr);

  EXPECT_TRUE(result.match_at(0)->has_tab_match.value_or(false));
  EXPECT_TRUE(result.match_at(1)->has_tab_match.value_or(false));
  EXPECT_FALSE(result.match_at(2)->has_tab_match.value_or(false));
}

TEST_F(AutocompleteResultTest, AttachesPedals) {
  FakeAutocompleteProviderClient client;
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    pedals.insert(
        std::make_pair(pedal->PedalId(), base::WrapRefCounted(pedal)));
  };
  add(new TestOmniboxPedalClearBrowsingData());
  client.set_pedal_provider(
      std::make_unique<OmniboxPedalProvider>(client, std::move(pedals)));
  EXPECT_NE(nullptr, client.GetPedalProvider());

  AutocompleteResult result;
  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  // Populate result with test matches.
  {
    ACMatches matches;
    struct TestData : AutocompleteMatchTestData {
      std::string contents;
      TestData(std::string url,
               AutocompleteMatch::Type type,
               std::string contents)
          : AutocompleteMatchTestData{url, type}, contents(contents) {}
    };
    const TestData data[] = {
        {"http://clear-history/", AutocompleteMatchType::SEARCH_SUGGEST,
         "clear history"},
        {"http://search-what-you-typed/",
         AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, "search what you typed"},
        {"http://search-history/", AutocompleteMatchType::SEARCH_HISTORY,
         "search history"},
        {"http://history-url/", AutocompleteMatchType::HISTORY_URL,
         "history url"},
        {"http://history-title/", AutocompleteMatchType::HISTORY_TITLE,
         "history title"},
        {"http://url-what-you-typed/",
         AutocompleteMatchType::URL_WHAT_YOU_TYPED, "url what you typed"},
        {"http://clipboard-url/", AutocompleteMatchType::CLIPBOARD_URL,
         "clipboard url"},
        {"http://bookmark-title/", AutocompleteMatchType::BOOKMARK_TITLE,
         "bookmark title"},
        {"http://entity-clear-history/",
         AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, "clear history"},
    };
    PopulateAutocompleteMatchesFromTestData(data, std::size(data), &matches);
    for (size_t i = 0; i < std::size(data); i++) {
      matches[i].contents = base::UTF8ToUTF16(data[i].contents);
    }
    result.AppendMatches(matches);
  }

  // Attach |pedal| to result matches where appropriate.
  result.AttachPedalsToMatches(input, client);

  // Ensure the entity suggestion doesn't get a pedal even though its contents
  // form a concept match.
  ASSERT_TRUE(std::prev(result.end())->actions.empty());

  // The same concept-matching contents on a non-entity suggestion gets a pedal.
  ASSERT_TRUE(!result.begin()->actions.empty());

  // Also ensure pedal can be retrieved with generic predicate.
  ASSERT_EQ(nullptr,
            std::prev(result.end())->GetActionWhere([](const auto& action) {
              return true;
            }));
  ASSERT_NE(nullptr, result.begin()->GetActionWhere([](const auto& action) {
    const auto* pedal = OmniboxPedal::FromAction(action.get());
    return pedal && pedal->PedalId() == OmniboxPedalId::CLEAR_BROWSING_DATA;
  }));

// Android & iOS avoid attaching tab-switch actions by design.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Include a tab-switch action, which is common and shouldn't prevent
  // pedals from attaching to the same match. The first match has a URL
  // that triggers tab-switch action attachment with this fake matcher.
  static_cast<FakeTabMatcher&>(const_cast<TabMatcher&>(client.GetTabMatcher()))
      .set_url_substring_match("clear-history");
  result.match_at(0)->actions.clear();
  result.match_at(0)->has_tab_match.reset();
  result.ConvertOpenTabMatches(&client, &input);
  EXPECT_EQ(result.match_at(0)->actions.size(), 1u);
  EXPECT_EQ(result.match_at(0)->GetActionAt(0u)->ActionId(),
            OmniboxActionId::TAB_SWITCH);
  result.AttachPedalsToMatches(input, client);
  EXPECT_EQ(result.match_at(0)->actions.size(), 2u);
  ASSERT_NE(nullptr, result.match_at(0)->GetActionWhere([](const auto& action) {
    const auto* pedal = OmniboxPedal::FromAction(action.get());
    return pedal && pedal->PedalId() == OmniboxPedalId::CLEAR_BROWSING_DATA;
  }));
#endif
}

TEST_F(AutocompleteResultTest, DocumentSuggestionsCanMergeButNotToDefault) {
  // Types are populated below to avoid introducing a new test data creation
  // process.
  TestData data[] = {
      {1, 4, 500, false},   // DOCUMENT result for url [1].
      {1, 1, 1100, false},  // HISTORY result for url [1], higher priority.
      {2, 4, 600, false},   // DOCUMENT result for [2].
      {2, 1, 1200, true},   // HISTORY result for url [2], higher priority,
                            // Can be default.
      {3, 4, 1000, false},  // DOCUMENT result for [3], higher priority
      {3, 1, 400, false},   // HISTORY result for url [3].
  };
  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  matches[0].type = AutocompleteMatchType::DOCUMENT_SUGGESTION;
  static_cast<FakeAutocompleteProvider*>(matches[0].provider)->type_ =
      AutocompleteProvider::Type::TYPE_DOCUMENT;
  matches[1].type = AutocompleteMatchType::HISTORY_URL;
  matches[2].type = AutocompleteMatchType::DOCUMENT_SUGGESTION;
  static_cast<FakeAutocompleteProvider*>(matches[2].provider)->type_ =
      AutocompleteProvider::Type::TYPE_DOCUMENT;
  matches[3].type = AutocompleteMatchType::HISTORY_URL;
  matches[4].type = AutocompleteMatchType::DOCUMENT_SUGGESTION;
  static_cast<FakeAutocompleteProvider*>(matches[4].provider)->type_ =
      AutocompleteProvider::Type::TYPE_DOCUMENT;
  matches[5].type = AutocompleteMatchType::HISTORY_URL;

  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  // We expect three results:
  // The document result for [1] may override the history result.
  // The document result for [2] cannot override a potentially-default result.
  // The document result for [3] is already higher-priority.
  EXPECT_EQ(result.size(), 3u);

  // First result should be the default with its original top-ranking score.
  EXPECT_EQ(result.match_at(0)->relevance, 1200);
  EXPECT_EQ(AutocompleteMatchType::HISTORY_URL, result.match_at(0)->type);
  EXPECT_TRUE(result.match_at(0)->allowed_to_be_default_match);

  // Second result should be a document result with elevated score.
  // The second DOCUMENT result is deduped and effectively dropped.
  EXPECT_EQ(result.match_at(1)->relevance, 1100);
  EXPECT_EQ(AutocompleteMatchType::DOCUMENT_SUGGESTION,
            result.match_at(1)->type);
  EXPECT_FALSE(result.match_at(1)->allowed_to_be_default_match);

  // Third result should be a document with original score. The history result
  // it duped against is lower-priority.
  EXPECT_EQ(result.match_at(2)->relevance, 1000);
  EXPECT_EQ(AutocompleteMatchType::DOCUMENT_SUGGESTION,
            result.match_at(2)->type);
  EXPECT_FALSE(result.match_at(2)->allowed_to_be_default_match);
}

TEST_F(AutocompleteResultTest, CalculateNumMatchesPerUrlCountTest) {
  CompareWithDemoteByType<AutocompleteMatch> comparison_object(
      metrics::OmniboxEventProto::OTHER);
  enum SuggestionType { search, url };

  auto test = [comparison_object](std::string base_limit,
                                  std::string url_cutoff,
                                  std::string increased_limit,
                                  std::vector<SuggestionType> types,
                                  size_t expected_num_matches) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{omnibox::kDynamicMaxAutocomplete,
          {{OmniboxFieldTrial::kDynamicMaxAutocompleteUrlCutoffParam,
            url_cutoff},
           {OmniboxFieldTrial::kDynamicMaxAutocompleteIncreasedLimitParam,
            increased_limit}}},
         {omnibox::kUIExperimentMaxAutocompleteMatches,
          {{OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, base_limit}}}},
        {});

    ACMatches matches;
    for (auto type : types) {
      AutocompleteMatch m;
      m.relevance = 100;
      if (type)
        m.type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
      matches.push_back(m);
    }
    const size_t num_matches = AutocompleteResult::CalculateNumMatches(
        false, matches, comparison_object);
    EXPECT_EQ(num_matches, expected_num_matches);
  };

  test("2", "0", "4", {search}, 1);
  test("2", "0", "4", {search, search, search, search, search}, 4);
  test("2", "0", "4", {search, search, search, search, url}, 4);
  test("2", "0", "4", {search, search, search, url, search}, 3);
  test("2", "0", "4", {search, search, url, search, search}, 2);
  test("2", "0", "4", {search, url, search, search, search}, 2);
  test("2", "1", "4", {search, url, search, search, search}, 4);
  test("2", "1", "4", {search, url, search, url, search}, 3);
}

TEST_F(AutocompleteResultTest, ClipboardSuggestionOnTopOfSearchSuggestionTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kGroupingFrameworkForNonZPS);

  // clang-format off
  TestData data[] = {
      {1, 1, 500,  false},
      {2, 2, 1100, false},
      {3, 2, 1000, false},
      {4, 1, 1300, false},
      {5, 1, 1500, false},
  };
  // clang-format on

  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);
  matches[0].type = AutocompleteMatchType::SEARCH_SUGGEST;
  static_cast<FakeAutocompleteProvider*>(matches[0].provider)->type_ =
      AutocompleteProvider::Type::TYPE_ZERO_SUGGEST_LOCAL_HISTORY;
  matches[1].type = AutocompleteMatchType::SEARCH_SUGGEST;
  static_cast<FakeAutocompleteProvider*>(matches[1].provider)->type_ =
      AutocompleteProvider::Type::TYPE_ZERO_SUGGEST_LOCAL_HISTORY;
  matches[2].type = AutocompleteMatchType::SEARCH_SUGGEST;
  static_cast<FakeAutocompleteProvider*>(matches[2].provider)->type_ =
      AutocompleteProvider::Type::TYPE_ZERO_SUGGEST_LOCAL_HISTORY;
  matches[3].type = AutocompleteMatchType::SEARCH_SUGGEST;
  static_cast<FakeAutocompleteProvider*>(matches[3].provider)->type_ =
      AutocompleteProvider::Type::TYPE_ZERO_SUGGEST_LOCAL_HISTORY;
  matches[4].type = AutocompleteMatchType::CLIPBOARD_URL;
  static_cast<FakeAutocompleteProvider*>(matches[4].provider)->type_ =
      AutocompleteProvider::Type::TYPE_CLIPBOARD;

  AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  AutocompleteResult result;
  result.AppendMatches(matches);
  result.SortAndCull(input, &template_url_service(),
                     triggered_feature_service());

  EXPECT_EQ(result.size(), 5u);
  EXPECT_EQ(result.match_at(0)->relevance, 1500);
  EXPECT_EQ(AutocompleteMatchType::CLIPBOARD_URL, result.match_at(0)->type);
}

TEST_F(AutocompleteResultTest, MaybeCullTailSuggestions) {
  auto test = [&](std::vector<CullTailTestMatch> input_matches) {
    ACMatches matches;
    base::ranges::transform(input_matches, std::back_inserter(matches),
                            [&](const CullTailTestMatch& test_match) {
                              AutocompleteMatch match;
                              match.contents = test_match.id;
                              match.type = test_match.type;
                              match.allowed_to_be_default_match =
                                  test_match.allowed_default;
                              match.relevance = 1000;
                              return match;
                            });

    auto page_classification = metrics::OmniboxEventProto::PageClassification::
        OmniboxEventProto_PageClassification_OTHER;
    AutocompleteResultForTesting::MaybeCullTailSuggestions(
        &matches, {page_classification});

    std::vector<CullTailTestMatch> output_matches;
    base::ranges::transform(
        matches, std::back_inserter(output_matches), [](const auto& match) {
          return CullTailTestMatch{match.contents, match.type,
                                   match.allowed_to_be_default_match};
        });
    return output_matches;
  };

  // When there are no suggestions, should return no suggestions.
  EXPECT_THAT(test({}), testing::ElementsAre());

  // T = tail, N = non-tail; D = default-able
  CullTailTestMatch t{u"T", AutocompleteMatchType::SEARCH_SUGGEST_TAIL, false};
  CullTailTestMatch n{u"N", AutocompleteMatchType::SEARCH_SUGGEST, false};
  CullTailTestMatch td{u"TD", AutocompleteMatchType::SEARCH_SUGGEST_TAIL, true};
  CullTailTestMatch nd{u"ND", AutocompleteMatchType::SEARCH_SUGGEST, true};

  // When there are only non-tail suggestions, no suggestions should be culled.
  EXPECT_THAT(test({n, n}), testing::ElementsAre(n, n));
  EXPECT_THAT(test({nd, nd}), testing::ElementsAre(nd, nd));
  EXPECT_THAT(test({nd, n}), testing::ElementsAre(nd, n));

  // When there are only tail suggestions, no suggestions should be culled.
  EXPECT_THAT(test({t, t}), testing::ElementsAre(t, t));
  EXPECT_THAT(test({td, td}), testing::ElementsAre(td, td));
  EXPECT_THAT(test({td, t}), testing::ElementsAre(td, t));

  // When there is exactly 1 non-tail suggestions and it is default-able, tail
  // suggestions should not be culled. But they should be prevented from being
  // default.
  // A tail suggest that was originally default-able (`td`), but was
  // prevented from being default.
  CullTailTestMatch tdp{u"TD", AutocompleteMatchType::SEARCH_SUGGEST_TAIL,
                        false};
  EXPECT_THAT(test({nd, t, t}), testing::ElementsAre(nd, t, t));
  EXPECT_THAT(test({nd, td, td}), testing::ElementsAre(nd, tdp, tdp));
  EXPECT_THAT(test({nd, td, t}), testing::ElementsAre(nd, tdp, t));

  // When there is exactly 1 non-tail suggestions and it is not default-able,
  // either it or the tail suggestions should be culled, depending on if there
  // is a default-able tail suggestion.
  EXPECT_THAT(test({n, t, t}), testing::ElementsAre(n));
  EXPECT_THAT(test({n, td, td}), testing::ElementsAre(td, td));
  EXPECT_THAT(test({n, td, t}), testing::ElementsAre(td, t));

  // When there are multiple non-tail suggestions, and at least 1 is
  // default-able, tail suggestions should be culled.
  EXPECT_THAT(test({nd, n, t}), testing::ElementsAre(nd, n));
  EXPECT_THAT(test({nd, n, td, td}), testing::ElementsAre(nd, n));
  EXPECT_THAT(test({nd, nd, td, t}), testing::ElementsAre(nd, nd));

  // When there are multiple non-tail suggestions, and none of them are
  // default-able, either they or the tail suggestions should be called,
  // depending on if there is a default-able tail suggestion.
  EXPECT_THAT(test({n, n, t, t}), testing::ElementsAre(n, n));
  EXPECT_THAT(test({n, n, td, td}), testing::ElementsAre(td, td));
  EXPECT_THAT(test({n, n, td, t}), testing::ElementsAre(td, t));

  // When there are both history cluster and tail suggestions, history cluster
  // suggestions should be hidden.
  // A history cluster suggestion.
  CullTailTestMatch h{u"H", AutocompleteMatchType::HISTORY_CLUSTER, false};
  EXPECT_THAT(test({nd, td, t, h}), testing::ElementsAre(nd, tdp, t));
}

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))

void VerifyTriggeredFeatures(
    OmniboxTriggeredFeatureService* triggered_feature_service,
    std::vector<OmniboxTriggeredFeatureService::Feature>
        expected_triggered_features) {
  OmniboxTriggeredFeatureService::Features features_triggered;
  OmniboxTriggeredFeatureService::Features features_triggered_in_session;
  triggered_feature_service->RecordToLogs(&features_triggered,
                                          &features_triggered_in_session);
  triggered_feature_service->ResetSession();
  EXPECT_THAT(features_triggered,
              testing::UnorderedElementsAreArray(expected_triggered_features));
  EXPECT_THAT(features_triggered_in_session,
              testing::UnorderedElementsAreArray(expected_triggered_features));
}

// NOTE: The tests below verify the behavior with the Grouping Framework for ZPS
// enabled. Suggestion groups only make sense within the Grouping Framework.
TEST_F(AutocompleteResultTest, Desktop_TwoColumnRealbox) {
  auto remote_secondary_zps_feature =
      metrics::OmniboxEventProto_Feature_REMOTE_SECONDARY_ZERO_SUGGEST;

  const auto group1 = omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST;
  const auto group2 = omnibox::GROUP_TRENDS;
  const auto group3 = omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS;
  TestData data[] = {
      {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
      {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
      {5, 1, 450, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group3},
      {6, 1, 440, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group3},
      {7, 1, 430, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group3},
  };
  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);

  // Suggestion groups have the omnibox::SECTION_DEFAULT and
  // omnibox::GroupConfig_SideType_DEFAULT_PRIMARY by default.
  omnibox::GroupConfigMap suggestion_groups_map;
  suggestion_groups_map[group1];
  suggestion_groups_map[group2];
  suggestion_groups_map[group3].set_side_type(
      omnibox::GroupConfig_SideType_SECONDARY);

  // Set up input for zero-prefix suggestions from the omnibox.
  AutocompleteInput omnibox_zps_input(
      u"",
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      TestSchemeClassifier());
  omnibox_zps_input.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);

  {
    SCOPED_TRACE("Query from omnibox");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {},
        {omnibox::kGroupingFrameworkForNonZPS, omnibox::kWebUIOmniboxPopup});
    AutocompleteResult result;
    result.MergeSuggestionGroupsMap(suggestion_groups_map);
    result.AppendMatches(matches);
    result.SortAndCull(omnibox_zps_input, &template_url_service(),
                       triggered_feature_service());

    const std::array<TestData, 5> expected_data{{
        // Previous search related suggestion chips are not permitted in the
        // omnibox even when the feature is enabled.
        {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
        {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
    }};
    AssertResultMatches(result, expected_data);

    // Verify that the secondary zero-prefix suggestions were not triggered.
    VerifyTriggeredFeatures(triggered_feature_service(), {});
  }
  {
    SCOPED_TRACE("Query from WebUI omnibox");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{omnibox::kWebUIOmniboxPopup, {}}},
        {omnibox::kGroupingFrameworkForNonZPS});
    AutocompleteResult result;
    result.MergeSuggestionGroupsMap(suggestion_groups_map);
    result.AppendMatches(matches);
    result.SortAndCull(omnibox_zps_input, &template_url_service(),
                       triggered_feature_service());

    const std::array<TestData, 8> expected_data{{
        // Previous search related suggestion chips are permitted in the omnibox
        // when the WebUI omnibox popup feature is enabled.
        {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
        {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
        {5, 1, 450, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group3},
        {6, 1, 440, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group3},
        {7, 1, 430, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group3},
    }};
    AssertResultMatches(result, expected_data);

    // Verify that the secondary zero-prefix suggestions were triggered.
    VerifyTriggeredFeatures(triggered_feature_service(),
                            {remote_secondary_zps_feature});
  }

  // Set up input for zero-prefix suggestions from the realbox.
  AutocompleteInput realbox_zps_input(
      u"", metrics::OmniboxEventProto::NTP_REALBOX, TestSchemeClassifier());
  realbox_zps_input.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);

  {
    SCOPED_TRACE("Query from realbox");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {},
        {omnibox::kGroupingFrameworkForNonZPS, omnibox::kWebUIOmniboxPopup});
    AutocompleteResult result;
    result.MergeSuggestionGroupsMap(suggestion_groups_map);
    result.AppendMatches(matches);
    result.SortAndCull(realbox_zps_input, &template_url_service(),
                       triggered_feature_service());

    const std::array<TestData, 8> expected_data{{
        {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
        {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
        {5, 1, 450, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group3},
        {6, 1, 440, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group3},
        {7, 1, 430, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group3},
    }};
    AssertResultMatches(result, expected_data);

    // Verify that the secondary zero-prefix suggestions were triggered.
    VerifyTriggeredFeatures(triggered_feature_service(),
                            {remote_secondary_zps_feature});
  }
  {
    SCOPED_TRACE("Query from realbox - no secondary matches");
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {},
        {omnibox::kGroupingFrameworkForNonZPS, omnibox::kWebUIOmniboxPopup});
    AutocompleteResult result;
    result.MergeSuggestionGroupsMap(suggestion_groups_map);
    // Clear the SideType_SECONDARY from the 3rd group.
    result.suggestion_groups_map_[group3].clear_side_type();
    result.AppendMatches(matches);
    result.SortAndCull(realbox_zps_input, &template_url_service(),
                       triggered_feature_service());

    const std::array<TestData, 5> expected_data{{
        // Previous search related suggestion chips not permitted when their
        // `SideType` is not SideType_Secondary.
        {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
        {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
    }};
    AssertResultMatches(result, expected_data);

    // Verify that the secondary zero-prefix suggestions were not triggered.
    VerifyTriggeredFeatures(triggered_feature_service(), {});
  }
}

TEST_F(AutocompleteResultTest, Desktop_ZpsGroupingIPH) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kStarterPackIPH);

  const auto group1 = omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST;
  const auto group2 = omnibox::GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP;
  TestData data[] = {
      {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {5, 1, 450, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {6, 1, 440, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {7, 1, 430, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {8,
       4,
       420,
       false,
       {},
       AutocompleteMatchType::NULL_RESULT_MESSAGE,
       group2,
       "",
       IphType::kFeaturedEnterpriseSearch},
  };
  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);

  // Suggestion groups have the omnibox::SECTION_DEFAULT by default.
  omnibox::GroupConfigMap suggestion_groups_map;
  suggestion_groups_map[group1];
  suggestion_groups_map[group2];

  // Set up input for zero-prefix suggestions from the omnibox.
  AutocompleteInput omnibox_zps_input(
      u"",
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      TestSchemeClassifier());
  omnibox_zps_input.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);

  {
    SCOPED_TRACE("Query from omnibox - with IPH");

    AutocompleteResult result;
    result.MergeSuggestionGroupsMap(suggestion_groups_map);
    result.AppendMatches(matches);
    result.SortAndCull(omnibox_zps_input, &template_url_service(),
                       triggered_feature_service());

    // There should be 8 total suggestions, including the IPH suggestion.
    // With the IPH suggestion present, the 8th group1 suggestion should be
    // culled in favor of the IPH suggestion.
    const std::array<TestData, 8> expected_data{{
        {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {5, 1, 450, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {6, 1, 440, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {8,
         4,
         420,
         false,
         {},
         AutocompleteMatchType::NULL_RESULT_MESSAGE,
         group2},
    }};
    AssertResultMatches(result, expected_data);
  }

  // Set up input for zero-prefix suggestions from the realbox.
  AutocompleteInput realbox_zps_input(
      u"", metrics::OmniboxEventProto::NTP_REALBOX, TestSchemeClassifier());
  realbox_zps_input.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);

  {
    SCOPED_TRACE("Query from realbox");
    AutocompleteResult result;
    result.MergeSuggestionGroupsMap(suggestion_groups_map);
    result.AppendMatches(matches);
    result.SortAndCull(realbox_zps_input, &template_url_service(),
                       triggered_feature_service());

    // The IPH suggestion should not be shown in the Realbox, even if it's
    // present in the list of matches.
    const std::array<TestData, 8> expected_data{{
        {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {5, 1, 450, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {6, 1, 440, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {7, 1, 430, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
    }};
    AssertResultMatches(result, expected_data);
  }

  {
    SCOPED_TRACE("Query from omnibox - without IPH");
    // Remove the IPH suggestion from the list of matches.
    matches.clear();
    PopulateAutocompleteMatches(data, std::size(data) - 1, &matches);

    AutocompleteResult result;
    result.MergeSuggestionGroupsMap(suggestion_groups_map);
    result.AppendMatches(matches);
    result.SortAndCull(omnibox_zps_input, &template_url_service(),
                       triggered_feature_service());

    // There should be 8 total suggestions, including the IPH suggestion.
    // With the IPH suggestion not present, all suggestion slots should be
    // filled with group1 suggestions.
    const std::array<TestData, 8> expected_data{{
        {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {5, 1, 450, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {6, 1, 440, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {7, 1, 430, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
    }};
    AssertResultMatches(result, expected_data);
  }
}

TEST_F(AutocompleteResultTest, SplitActionsToSuggestions) {
  FakeAutocompleteProviderClient client;
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    pedals.insert(
        std::make_pair(pedal->PedalId(), base::WrapRefCounted(pedal)));
  };
  add(new TestOmniboxPedalClearBrowsingData());
  client.set_pedal_provider(
      std::make_unique<OmniboxPedalProvider>(client, std::move(pedals)));
  EXPECT_NE(nullptr, client.GetPedalProvider());

  AutocompleteResult result;
  AutocompleteInput input(u"a", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());

  {
    ACMatches matches;
    struct TestData : AutocompleteMatchTestData {
      std::string contents;
      TestData(std::string url,
               AutocompleteMatch::Type type,
               std::string contents)
          : AutocompleteMatchTestData{url, type}, contents(contents) {}
    };
    const TestData data[] = {
        {"http://clear-history/", AutocompleteMatchType::SEARCH_SUGGEST,
         "clear history"},
        {"http://search-what-you-typed/",
         AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, "search what you typed"},
        {"http://search-history/", AutocompleteMatchType::SEARCH_HISTORY,
         "search history"},
        {"http://history-url/", AutocompleteMatchType::HISTORY_URL,
         "history url"},
    };
    PopulateAutocompleteMatchesFromTestData(data, std::size(data), &matches);
    for (size_t i = 0; i < std::size(data); i++) {
      matches[i].contents = base::UTF8ToUTF16(data[i].contents);
    }
    result.AppendMatches(matches);
  }

  // First, the pedal is attached as normal.
  result.AttachPedalsToMatches(input, client);
  EXPECT_TRUE(!result.begin()->actions.empty());
  EXPECT_EQ(nullptr, result.match_at(1)->takeover_action);
  EXPECT_EQ(result.size(), 4u);

  // Then pedals are split out to dedicated suggestions with takeover action.
  // Note that by design, number of results is not changed.
  result.SplitActionsToSuggestions();
  EXPECT_TRUE(result.begin()->actions.empty());
  EXPECT_NE(nullptr, result.match_at(1)->takeover_action);
  EXPECT_EQ(result.size(), 4u);

  // Now for an artifically exaggerated case with two pedals on one match,
  // which doesn't happen naturally but is useful for testing the method.
  static_cast<FakeTabMatcher&>(const_cast<TabMatcher&>(client.GetTabMatcher()))
      .set_url_substring_match("clear-history");
  result.AttachPedalsToMatches(input, client);
  EXPECT_EQ(result.match_at(0)->actions.size(), 1u);
  result.match_at(0)->has_tab_match.reset();
  result.ConvertOpenTabMatches(&client, &input);
  EXPECT_EQ(result.match_at(0)->actions.size(), 2u);
  EXPECT_EQ(result.match_at(0)->GetActionAt(1u)->ActionId(),
            OmniboxActionId::TAB_SWITCH);
  result.match_at(0)->actions.push_back(result.match_at(0)->GetActionAt(0u));
  EXPECT_EQ(result.match_at(0)->actions.size(), 3u);
  // We have three actions: pedal, tab-switch, pedal. Split and ensure
  // both pedals became dedicated suggestions. The first one from above
  // is still there and is not affected by splitting again.
  result.SplitActionsToSuggestions();
  EXPECT_EQ(result.match_at(0)->actions.size(), 1u);
  EXPECT_EQ(result.match_at(0)->GetActionAt(0u)->ActionId(),
            OmniboxActionId::TAB_SWITCH);
  EXPECT_EQ(result.match_at(1)->takeover_action->ActionId(),
            OmniboxActionId::PEDAL);
  EXPECT_EQ(result.match_at(2)->takeover_action->ActionId(),
            OmniboxActionId::PEDAL);
  EXPECT_EQ(result.match_at(3)->takeover_action->ActionId(),
            OmniboxActionId::PEDAL);
  EXPECT_EQ(result.size(), 4u);
}

#endif  // !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))

#if BUILDFLAG(IS_ANDROID)
TEST_F(AutocompleteResultTest, Android_InspireMe) {
  const auto group1 = omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST;
  const auto group2 = omnibox::GROUP_TRENDS;
  const auto group3 = omnibox::GROUP_PREVIOUS_SEARCH_RELATED;
  TestData data[] = {
      {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {1, 1, 490, true, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
      {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
      {5, 1, 450, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group3},
      {6, 1, 440, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group3},
  };
  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);

  // Suggestion groups have the omnibox::SECTION_DEFAULT and
  // omnibox::GroupConfig_SideType_DEFAULT_PRIMARY by default.
  omnibox::GroupConfigMap suggestion_groups_map;
  suggestion_groups_map[group1];
  suggestion_groups_map[group2];
  suggestion_groups_map[group3];

  // Set up input for zero-prefix suggestions.
  AutocompleteInput zero_input(u"", metrics::OmniboxEventProto::NTP,
                               TestSchemeClassifier());
  zero_input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);

  // NOTE:
  // The tests below verify the behavior with the Grouping Framework for ZPS
  // enabled. This is intentional: Suggestion Groups make no sense outside of
  // the grouping framework.

  {
    SCOPED_TRACE("Inspire Me Passes Only Trending Queries");
    AutocompleteResult result;
    result.MergeSuggestionGroupsMap(suggestion_groups_map);
    result.AppendMatches(matches);
    result.SortAndCull(zero_input, &template_url_service(),
                       triggered_feature_service());

    const std::array<TestData, 5> expected_data{{
        // Default suggestion comes 1st.
        {1, 1, 490, true, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        // Other types include all of the Inspire Me queries.
        {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
        {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
        {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
    }};
    AssertResultMatches(result, expected_data);
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(AutocompleteResultTest, Android_UndedupTopSearch) {
  scoped_refptr<FakeAutocompleteProvider> provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_SEARCH);

  // 4 different matches to cover variety of scenarios.
  // Matches are recognized by their type and actions presence.
  // `search` is marked as a duplicate of both `entity` matches.
  AutocompleteMatch what_you_typed(
      provider.get(), 1, false, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  AutocompleteMatch search(provider.get(), 1, false,
                           AutocompleteMatchType::SEARCH_SUGGEST);
  search.allowed_to_be_default_match = true;

  AutocompleteMatch entity_without_action(
      provider.get(), 1, false, AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);
  AutocompleteMatch entity_with_action(
      provider.get(), 1, false, AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);
  entity_with_action.actions.push_back(base::MakeRefCounted<FakeOmniboxAction>(
      OmniboxActionId::ACTION_IN_SUGGEST));
  entity_with_action.duplicate_matches.push_back(search);
  entity_without_action.duplicate_matches.push_back(search);

  struct UndedupTestData {
    std::string test_name;
    bool promote_entities;
    std::vector<AutocompleteMatch> input;
    std::vector<AutocompleteMatch> expected_result;
  } test_cases[]{
      {"no op with no matches", true, {}, {}},
      {"no op with no entities / 1", true, {what_you_typed}, {what_you_typed}},
      {"no op with no entities / 2",
       true,
       {what_you_typed, search},
       {what_you_typed, search}},
      {"no op with entities with no actions",
       true,
       {what_you_typed, entity_without_action},
       {what_you_typed, entity_without_action}},
      {"no op with entities with actions at low positions",
       true,
       {what_you_typed, entity_with_action},
       {what_you_typed, entity_with_action}},

      // Undedup and possibly rotate eligible cases.
      {"no rotation when promotion is disabled with no actions at top position",
       false,
       {entity_without_action},
       {search, entity_without_action}},
      {"no rotation when promotion is enabled with no actions at top position",
       true,
       {entity_without_action},
       {search, entity_without_action}},
      {"no rotation when promotion is disabled with actions at top position",
       false,
       {entity_with_action},
       {search, entity_with_action}},
      {"rotation when promotion is enabled with actions at top position",
       true,
       {entity_with_action},
       {entity_with_action, search}},
  };

  // Crete matches following the `input_matches_and_actions` input.
  // The input specifies what type of OMNIBOX_ACTION should be added to every
  // individual match.
  // Once done, run the trimming and verify that the output contains exactly the
  // matches we want to see.
  for (const auto& test_case : test_cases) {
    auto result = test_case.input;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kActionsInSuggest,
        {{OmniboxFieldTrial::kActionsInSuggestPromoteEntitySuggestion.name,
          test_case.promote_entities ? "true" : "false"}});
    AutocompleteResult::UndedupTopSearchEntityMatch(&result);

    EXPECT_EQ(result.size(), test_case.expected_result.size());
    for (size_t index = 0u; index < result.size(); ++index) {
      const auto& found_match = result[index];
      const auto& expect_match = test_case.expected_result[index];
      EXPECT_EQ(found_match.type, expect_match.type)
          << "at index " << index
          << " while testing variant: " << test_case.test_name;
      EXPECT_EQ(found_match.actions.size(), expect_match.actions.size())
          << "at index " << index
          << " while testing variant: " << test_case.test_name;
      for (size_t action_index = 0u; action_index < found_match.actions.size();
           ++action_index) {
        EXPECT_EQ(found_match.actions[action_index]->ActionId(),
                  expect_match.actions[action_index]->ActionId())
            << "action " << action_index << " at index " << index
            << " while testing variant: " << test_case.test_name;
      }
    }
  }
}

#if BUILDFLAG(IS_IOS)
TEST_F(AutocompleteResultTest, IOS_InspireMe) {
  const auto group1 = omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST;
  const auto group2 = omnibox::GROUP_TRENDS;
  TestData data[] = {
      {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
      {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
  };
  ACMatches matches;
  PopulateAutocompleteMatches(data, std::size(data), &matches);

  // Suggestion groups have the omnibox::SECTION_DEFAULT and
  // omnibox::GroupConfig_SideType_DEFAULT_PRIMARY by default.
  omnibox::GroupConfigMap suggestion_groups_map;
  suggestion_groups_map[group1];
  suggestion_groups_map[group2];

  // Set up input for zero-prefix suggestions.
  AutocompleteInput zero_input(u"", metrics::OmniboxEventProto::NTP,
                               TestSchemeClassifier());
  zero_input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);

  {
    SCOPED_TRACE("Trend suggestions are only available on iPhones");
    base::test::ScopedFeatureList feature_list;
    AutocompleteResult result;
    result.MergeSuggestionGroupsMap(suggestion_groups_map);
    result.AppendMatches(matches);
    result.SortAndCull(zero_input, &template_url_service(),
                       triggered_feature_service());

    if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
      // Ipads should keep the default config.
      const std::array<TestData, 3> expected_data{{
          {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
          {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
          {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
      }};
      AssertResultMatches(result, expected_data);
    } else {
      const std::array<TestData, 5> expected_data{{
          {0, 1, 500, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
          {1, 1, 490, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
          {2, 1, 480, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group1},
          {3, 1, 470, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
          {4, 1, 460, false, {}, AutocompleteMatchType::SEARCH_SUGGEST, group2},
      }};
      AssertResultMatches(result, expected_data);
    }
  }
}
#endif

#if (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))

TEST_F(AutocompleteResultTest, Mobile_TrimOmniboxActions) {
  scoped_refptr<FakeAutocompleteProvider> provider =
      new FakeAutocompleteProvider(AutocompleteProvider::Type::TYPE_SEARCH);
  using OmniboxActionId::ACTION_IN_SUGGEST;
  using OmniboxActionId::ANSWER_ACTION;
  using OmniboxActionId::PEDAL;
  using OmniboxActionId::UNKNOWN;
  const std::set<OmniboxActionId> all_actions_to_test{ACTION_IN_SUGGEST, PEDAL};

  struct FilterOmniboxActionsTestData {
    std::string test_name;
    std::vector<std::vector<OmniboxActionId>> input_matches_and_actions;
    std::vector<std::vector<OmniboxActionId>> result_matches_and_actions_zps;
    std::vector<std::vector<OmniboxActionId>> result_matches_and_actions_typed;
    bool include_url = false;
  } test_cases[]{
      {"No actions attached to matches",
       {{}, {}, {}, {}},
       {{}, {}, {}, {}},
       {{}, {}, {}, {}}},
      {"Pedals shown only in top three slots",
       {{PEDAL}, {PEDAL}, {PEDAL}, {PEDAL}},
       // ZPS
       {{PEDAL}, {PEDAL}, {PEDAL}, {}},
       // Typed
       {{PEDAL}, {PEDAL}, {PEDAL}, {}}},
      {"Actions are shown only in first position",
       {{ACTION_IN_SUGGEST},
        {ACTION_IN_SUGGEST},
        {ACTION_IN_SUGGEST},
        {ACTION_IN_SUGGEST}},
       // ZPS
       {{}, {}, {}, {}},
       // Typed
       {{ACTION_IN_SUGGEST}, {}, {}, {}}},
      {"Actions are promoted over Pedals; positions dictate preference",
       {{ACTION_IN_SUGGEST, PEDAL},
        {ACTION_IN_SUGGEST, PEDAL},
        {ACTION_IN_SUGGEST, PEDAL},
        {ACTION_IN_SUGGEST, PEDAL}},
       // ZPS
       {{PEDAL}, {PEDAL}, {PEDAL}, {}},
       // Typed
       {{ACTION_IN_SUGGEST}, {PEDAL}, {PEDAL}, {}}},
      {"Actions are promoted over History clusters; positions dictate "
       "preference",
       {{ACTION_IN_SUGGEST, PEDAL},
        {ACTION_IN_SUGGEST, PEDAL},
        {ACTION_IN_SUGGEST, PEDAL},
        {ACTION_IN_SUGGEST, PEDAL}},
       // ZPS
       {{PEDAL}, {PEDAL}, {PEDAL}, {}},
       // Typed
       {{ACTION_IN_SUGGEST}, {PEDAL}, {PEDAL}, {}}},
      {"Answer actions promoted over pedals; can go in any position",
       {{ANSWER_ACTION, PEDAL},
        {ANSWER_ACTION, PEDAL},
        {ANSWER_ACTION, PEDAL},
        {ANSWER_ACTION, PEDAL}},
       // ZPS
       {{ANSWER_ACTION}, {ANSWER_ACTION}, {ANSWER_ACTION}, {ANSWER_ACTION}},
       // Typed
       {{ANSWER_ACTION}, {ANSWER_ACTION}, {ANSWER_ACTION}, {ANSWER_ACTION}}},
      {"Answer actions suppressed when there are urls",
       {{PEDAL, ANSWER_ACTION},
        {ANSWER_ACTION},
        {ANSWER_ACTION},
        {ANSWER_ACTION}},
       // ZPS
       {{PEDAL}, {}, {}, {}},
       // Typed
       {{PEDAL}, {}, {}, {}},
       /* include_url= */ true},
  };

  // Crete matches following the `input_matches_and_actions` input.
  // The input specifies what type of OMNIBOX_ACTION should be added to every
  // individual match.
  // Once done, run the trimming and verify that the output contains exactly the
  // matches we want to see.
  auto run_test = [&](const FilterOmniboxActionsTestData& data) {
    // Create AutocompleteResult from the test data
    AutocompleteResult zps_result;
    AutocompleteResult typed_result;
    for (const auto& actions : data.input_matches_and_actions) {
      AutocompleteMatch match(
          provider.get(), 1, false,
          data.include_url ? AutocompleteMatchType::URL_WHAT_YOU_TYPED
                           : AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);
      for (auto& action_id : actions) {
        if (action_id == OmniboxActionId::ACTION_IN_SUGGEST) {
          omnibox::ActionInfo info;
          info.set_action_type(omnibox::ActionInfo_ActionType_DIRECTIONS);
          match.actions.push_back(base::MakeRefCounted<OmniboxActionInSuggest>(
              std::move(info), std::nullopt));
        } else {
          match.actions.push_back(
              base::MakeRefCounted<FakeOmniboxAction>(action_id));
        }
      }
      zps_result.AppendMatches({match});
      typed_result.AppendMatches({match});
    }

    auto check_results =
        [&](AutocompleteResult& result,
            std::vector<std::vector<OmniboxActionId>> expected_actions) {
          // Check results.
          EXPECT_EQ(result.size(), expected_actions.size())
              << "while testing variant: " << data.test_name;

          for (size_t index = 0u; index < result.size(); ++index) {
            const auto* match = result.match_at(index);
            const auto& expected_actions_at_position = expected_actions[index];
            EXPECT_EQ(match->actions.size(),
                      expected_actions_at_position.size());
            for (size_t action_index = 0u;
                 action_index < expected_actions_at_position.size();
                 ++action_index) {
              EXPECT_EQ(expected_actions_at_position[action_index],
                        match->actions[action_index]->ActionId())
                  << "match " << index << "action " << action_index
                  << " while testing variant: " << data.test_name;
            }
          }
        };

    // Run the trimmer. ZPS, then typed.
    zps_result.TrimOmniboxActions(true);
    check_results(zps_result, data.result_matches_and_actions_zps);

    typed_result.TrimOmniboxActions(false);
    check_results(typed_result, data.result_matches_and_actions_typed);
  };

  for (const auto& test_case : test_cases) {
    run_test(test_case);
  }
}

#endif
