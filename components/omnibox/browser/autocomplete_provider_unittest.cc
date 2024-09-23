// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/omnibox/browser/autocomplete_provider.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/fake_clipboard_recent_content.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/image/image_util.h"
#include "url/url_constants.h"

namespace {

const size_t kResultsPerProvider = 3;
const char16_t kTestTemplateURLKeyword[] = u"t";

class TestingSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  TestingSchemeClassifier() = default;
  TestingSchemeClassifier(const TestingSchemeClassifier&) = delete;
  TestingSchemeClassifier& operator=(const TestingSchemeClassifier&) = delete;

  metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const override {
    DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
    return (scheme == url::kHttpScheme || scheme == url::kHttpsScheme)
               ? metrics::OmniboxInputType::URL
               : metrics::OmniboxInputType::EMPTY;
  }
};

// AutocompleteController::Observer implementation that runs the provided
// closure, when the controller is done.
class TestAutocompleteControllerObserver
    : public AutocompleteController::Observer {
 public:
  TestAutocompleteControllerObserver() = default;
  ~TestAutocompleteControllerObserver() override = default;
  TestAutocompleteControllerObserver(
      const TestAutocompleteControllerObserver&) = delete;
  TestAutocompleteControllerObserver& operator=(
      const TestAutocompleteControllerObserver&) = delete;

  void set_closure(const base::RepeatingClosure& closure) {
    closure_ = closure;
  }

  void set_is_observing() { is_observing_ = true; }
  bool is_observing() const { return is_observing_; }

 private:
  // AutocompleteController::Observer implementation.
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override {
    if (controller->done() && closure_) {
      closure_.Run();
    }
  }
  base::RepeatingClosure closure_;
  bool is_observing_ = false;
};

// AutocompleteProviderListener implementation that runs the provided closure,
// when the provider is done. Informs the controller for non-prefetch requests.
class TestAutocompleteProviderListener : public AutocompleteProviderListener {
 public:
  explicit TestAutocompleteProviderListener(AutocompleteController* controller)
      : controller_(controller) {}
  ~TestAutocompleteProviderListener() override = default;
  TestAutocompleteProviderListener(const TestAutocompleteProviderListener&) =
      delete;
  TestAutocompleteProviderListener& operator=(
      const TestAutocompleteProviderListener&) = delete;

  void set_closure(const base::RepeatingClosure& closure) {
    closure_ = closure;
  }

  // TestAutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override {
    controller_->OnProviderUpdate(updated_matches, provider);
    if (closure_)
      closure_.Run();
  }

  // Used by TestProvider to notify it is done with a prefetch request.
  void OnProviderFinishedPrefetch() {
    if (closure_)
      closure_.Run();
  }

 private:
  raw_ptr<AutocompleteController> controller_;
  base::RepeatingClosure closure_;
};

}  // namespace

// Autocomplete provider that provides known results. Note that this is
// refcounted so that it can also be a task on the message loop.
class TestProvider : public AutocompleteProvider {
 public:
  TestProvider(int relevance,
               const std::u16string& prefix,
               const std::u16string& match_keyword,
               AutocompleteProviderClient* client)
      : AutocompleteProvider(AutocompleteProvider::TYPE_SEARCH),
        relevance_(relevance),
        prefix_(prefix),
        match_keyword_(match_keyword),
        client_(client) {}
  TestProvider(const TestProvider&) = delete;
  TestProvider& operator=(const TestProvider&) = delete;

  // AutocompleteProvider:
  void StartPrefetch(const AutocompleteInput& input) override;
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void ResizeMatches(size_t max_matches, bool ml_scoring_enabled);

  void set_supports_prefetch(const bool supports_prefetch) {
    supports_prefetch_ = supports_prefetch;
  }

  bool prefetch_done() { return prefetch_done_; }

  void set_closure(const base::RepeatingClosure& closure) {
    closure_ = closure;
  }

  void set_matches(const ACMatches& matches) { matches_ = matches; }
  const ACMatches& get_matches() { return matches_; }

 protected:
  ~TestProvider() override = default;

  void OnNonPrefetchRequestDone();
  void OnPrefetchRequestDone();

  void AddResults(int start_at, int num);
  void AddResultsWithSearchTermsArgs(
      int start_at,
      int num,
      AutocompleteMatch::Type type,
      const TemplateURLRef::SearchTermsArgs& search_terms_args);

  int relevance_;
  const std::u16string prefix_;
  const std::u16string match_keyword_;
  raw_ptr<AutocompleteProviderClient> client_;
  bool supports_prefetch_{false};
  bool prefetch_done_{true};
  base::RepeatingClosure closure_;
};

void TestProvider::StartPrefetch(const AutocompleteInput& input) {
  if (!supports_prefetch_) {
    return;
  }

  matches_.clear();
  prefetch_done_ = false;
  if (closure_) {
    closure_.Run();
  }

  if (!input.omit_asynchronous_matches()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&TestProvider::OnPrefetchRequestDone,
                                  base::Unretained(this)));
  } else {
    OnPrefetchRequestDone();
  }
}

void TestProvider::OnPrefetchRequestDone() {
  AddResults(0, kResultsPerProvider);
  prefetch_done_ = true;
  for (AutocompleteProviderListener* listener : listeners_) {
    static_cast<TestAutocompleteProviderListener*>(listener)
        ->OnProviderFinishedPrefetch();
  }
}

void TestProvider::Start(const AutocompleteInput& input, bool minimal_changes) {
  if (minimal_changes)
    return;

  matches_.clear();

  if (input.IsZeroSuggest()) {
    return;
  }

  // Generate 4 results synchronously, the rest later.
  AddResults(0, 1);
  AddResultsWithSearchTermsArgs(1, 1,
                                AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                                TemplateURLRef::SearchTermsArgs(u"echo"));
  AddResultsWithSearchTermsArgs(2, 1, AutocompleteMatchType::NAVSUGGEST,
                                TemplateURLRef::SearchTermsArgs(u"nav"));
  AddResultsWithSearchTermsArgs(3, 1, AutocompleteMatchType::SEARCH_SUGGEST,
                                TemplateURLRef::SearchTermsArgs(u"query"));

  if (!input.omit_asynchronous_matches()) {
    done_ = false;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&TestProvider::OnNonPrefetchRequestDone,
                                  base::Unretained(this)));
  }
}

void TestProvider::ResizeMatches(size_t max_matches, bool ml_scoring_enabled) {
  AutocompleteProvider::ResizeMatches(max_matches, ml_scoring_enabled);
}

void TestProvider::OnNonPrefetchRequestDone() {
  AddResults(1, kResultsPerProvider);
  done_ = true;
  NotifyListeners(true);
}

void TestProvider::AddResults(int start_at, int num) {
  AddResultsWithSearchTermsArgs(
      start_at, num, AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      TemplateURLRef::SearchTermsArgs(std::u16string()));
}

void TestProvider::AddResultsWithSearchTermsArgs(
    int start_at,
    int num,
    AutocompleteMatch::Type type,
    const TemplateURLRef::SearchTermsArgs& search_terms_args) {
  for (int i = start_at; i < num; i++) {
    AutocompleteMatch match(this, relevance_ - i, false, type);

    match.fill_into_edit = prefix_ + base::UTF8ToUTF16(base::NumberToString(i));
    match.destination_url = GURL(base::UTF16ToUTF8(match.fill_into_edit));
    match.allowed_to_be_default_match = true;

    match.contents = match.fill_into_edit;
    match.contents_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    match.description = match.fill_into_edit;
    match.description_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(search_terms_args);
    if (!match_keyword_.empty()) {
      match.keyword = match_keyword_;
      ASSERT_NE(nullptr,
                match.GetTemplateURL(client_->GetTemplateURLService(), false));
    }

    matches_.push_back(match);
  }
}

// Helper class to make running tests of ClassifyAllMatchesInString() more
// convenient.
class ClassifyTest {
 public:
  ClassifyTest(const std::u16string& text,
               const bool text_is_query,
               ACMatchClassifications matches);
  ~ClassifyTest();

  ACMatchClassifications RunTest(const std::u16string& find_text);

 private:
  const std::u16string text_;
  const bool text_is_query_;
  const ACMatchClassifications matches_;
};

ClassifyTest::ClassifyTest(const std::u16string& text,
                           const bool text_is_query,
                           ACMatchClassifications matches)
    : text_(text), text_is_query_(text_is_query), matches_(matches) {}

ClassifyTest::~ClassifyTest() = default;

ACMatchClassifications ClassifyTest::RunTest(const std::u16string& find_text) {
  return ClassifyAllMatchesInString(find_text, text_, text_is_query_, matches_);
}

class AutocompleteProviderTest : public testing::Test {
 public:
  AutocompleteProviderTest();
  ~AutocompleteProviderTest() override;
  AutocompleteProviderTest(const AutocompleteProviderTest&) = delete;
  AutocompleteProviderTest& operator=(const AutocompleteProviderTest&) = delete;

  void CopyResults();

 protected:
  struct KeywordTestData {
    const std::u16string fill_into_edit;
    const std::u16string keyword;
    const std::u16string expected_associated_keyword;
  };

  struct SuggestionGroupsTestData {
    omnibox::GroupConfigMap suggestion_groups_map;
    std::vector<std::optional<omnibox::GroupId>> suggestion_group_ids;
  };

  struct SearchboxStatsTestData {
    const AutocompleteMatch::Type match_type;
    std::optional<omnibox::GroupId> group_id;
    const omnibox::metrics::ChromeSearchboxStats expected_searchbox_stats;
    omnibox::SuggestType type;
    base::flat_set<omnibox::SuggestSubtype> subtypes;
  };

  // Registers a test TemplateURL under the given keyword.
  void RegisterTemplateURL(const std::u16string& keyword,
                           const std::string& template_url,
                           const std::string& image_url,
                           const std::string& image_url_post_params);

  // Resets |controller_| with two TestProviders.  |provider1_ptr| and
  // |provider2_ptr| are updated to point to the new providers if non-NULL.
  void ResetControllerWithTestProviders(bool same_destinations,
                                        TestProvider** provider1_ptr,
                                        TestProvider** provider2_ptr);

  // Runs a query on the input "a", and makes sure both providers' input is
  // properly collected.
  void RunTest();

  // Constructs an AutocompleteResult from |match_data|, sets the |controller_|
  // to pretend it was running against input |input|, calls the |controller_|'s
  // UpdateAssociatedKeywords, and checks that the matches have associated
  // keywords as expected.
  void RunKeywordTest(const std::u16string& input,
                      const KeywordTestData* match_data,
                      size_t size);

  void UpdateResultsWithSuggestionGroupsTestData(
      const SuggestionGroupsTestData& test_data);

  void RunSearchboxStatsTest(const SearchboxStatsTestData* sbs_test_data,
                             size_t size,
                             bool input_is_zero_suggest);

  void RunQuery(const std::string& query, bool allow_exact_keyword_match);

  void ResetControllerWithKeywordAndSearchProviders();
  void ResetControllerWithKeywordProvider();
  void RunExactKeymatchTest(bool allow_exact_keyword_match);

  // Returns match.destination_url as it would be set by
  // AutocompleteController::UpdateMatchDestinationURL().
  GURL GetDestinationURL(AutocompleteMatch& match,
                         base::TimeDelta query_formulation_time) const;

  void set_remote_search_feature_triggered_in_session(bool value) {
    client_->GetOmniboxTriggeredFeatureService()->ResetSession();
    if (value) {
      client_->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
          metrics::OmniboxEventProto_Feature_REMOTE_SEARCH_FEATURE);
    }
  }

  void set_current_page_classification(
      metrics::OmniboxEventProto::PageClassification classification) {
    controller_->input_.current_page_classification_ = classification;
  }
  void add_zero_suggest_provider_experiment_stats_v2(
      const omnibox::metrics::ChromeSearchboxStats::ExperimentStatsV2&
          experiment_stat_v2) {
    auto& experiment_stats_v2s =
        const_cast<SearchSuggestionParser::ExperimentStatsV2s&>(
            controller_->zero_suggest_provider_->experiment_stats_v2s());
    experiment_stats_v2s.push_back(experiment_stat_v2);
  }

  PrefService* GetPrefs() {
    return &search_engines_test_environment_.pref_service();
  }

  // Resets the controller with the given |type|. |type| is a bitmap containing
  // AutocompleteProvider::Type values that will (potentially, depending on
  // platform, flags, etc.) be instantiated.
  void ResetControllerWithType(int type);

  base::test::TaskEnvironment task_environment_;
  TestAutocompleteControllerObserver autocomplete_controller_observer_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;

  std::unique_ptr<AutocompleteController> controller_;
  // Owned by |controller_|.
  raw_ptr<MockAutocompleteProviderClient> client_;
  // `result_` may contain a `raw_ptr` (e.g. `AutocompleteMatch::provider) to
  // the `controller_`.  This means that (per //docs/dangling_ptr_guide.md) the
  // `result_` field needs to be declared *after* the `controller_` field.
  AutocompleteResult result_;
  // Used to ensure that |client_| ownership has been passed to |controller_|
  // exactly once.
  bool client_owned_{};
};

AutocompleteProviderTest::AutocompleteProviderTest()
    : client_(new MockAutocompleteProviderClient()) {
  client_->set_template_url_service(
      search_engines_test_environment_.template_url_service());
}

AutocompleteProviderTest::~AutocompleteProviderTest() {
  EXPECT_TRUE(client_owned_);
}

void AutocompleteProviderTest::RegisterTemplateURL(
    const std::u16string& keyword,
    const std::string& template_url,
    const std::string& image_url = "",
    const std::string& image_url_post_params = "") {
  TemplateURLData data;
  data.SetURL(template_url);
  data.SetShortName(keyword);
  data.SetKeyword(keyword);
  data.image_url = image_url;
  data.image_url_post_params = image_url_post_params;
  TemplateURLService* turl_model = client_->GetTemplateURLService();
  TemplateURL* default_turl =
      turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_turl);
  turl_model->Load();
  TemplateURLID default_provider_id = default_turl->id();
  ASSERT_NE(0, default_provider_id);
}

void AutocompleteProviderTest::ResetControllerWithTestProviders(
    bool same_destinations,
    TestProvider** provider1_ptr,
    TestProvider** provider2_ptr) {
  // TODO: Move it outside this method, after refactoring the existing
  // unit tests.  Specifically:
  //   (1) Make sure that AutocompleteMatch.keyword is set iff there is
  //       a corresponding call to RegisterTemplateURL; otherwise the
  //       controller flow will crash; this practically means that
  //       RunTests/ResetControllerXXX/RegisterTemplateURL should
  //       be coordinated with each other.
  //   (2) Inject test arguments rather than rely on the hardcoded values, e.g.
  //       don't rely on kResultsPerProvided and default relevance ordering
  //       (B > A).
  RegisterTemplateURL(kTestTemplateURLKeyword,
                      "http://foo/{searchTerms}/{google:assistedQueryStats}");

  AutocompleteController::Providers providers;

  // Construct two new providers, with either the same or different prefixes.
  TestProvider* provider1 = new TestProvider(kResultsPerProvider, u"http://a",
                                             kTestTemplateURLKeyword, client_);
  providers.push_back(provider1);

  TestProvider* provider2 = new TestProvider(
      kResultsPerProvider * 2, same_destinations ? u"http://a" : u"http://b",
      std::u16string(), client_);
  providers.push_back(provider2);

  // Reset the controller to contain our new providers.
  ResetControllerWithType(0);

  // We're going to swap the providers vector, but the old vector should be
  // empty so no elements need to be freed at this point.
  EXPECT_TRUE(controller_->providers_.empty());
  controller_->providers_.swap(providers);
  provider1->AddListener(controller_.get());
  provider2->AddListener(controller_.get());

  if (provider1_ptr)
    *provider1_ptr = provider1;
  if (provider2_ptr)
    *provider2_ptr = provider2;
}

void AutocompleteProviderTest::ResetControllerWithKeywordAndSearchProviders() {
  // Reset the default TemplateURL.
  TemplateURLData data;
  data.SetShortName(u"default");
  data.SetKeyword(u"default");
  data.SetURL("http://defaultturl/{searchTerms}");
  TemplateURLService* turl_model = client_->GetTemplateURLService();
  TemplateURL* default_turl =
      turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_turl);
  TemplateURLID default_provider_id = default_turl->id();
  ASSERT_NE(0, default_provider_id);

  // Create another TemplateURL for KeywordProvider.
  TemplateURLData data2;
  data2.SetShortName(u"k");
  data2.SetKeyword(u"k");
  data2.SetURL("http://keyword/{searchTerms}");
  TemplateURL* keyword_turl =
      turl_model->Add(std::make_unique<TemplateURL>(data2));
  ASSERT_NE(0, keyword_turl->id());

  ResetControllerWithType(AutocompleteProvider::TYPE_KEYWORD |
                          AutocompleteProvider::TYPE_SEARCH |
                          AutocompleteProvider::TYPE_ZERO_SUGGEST);
}

void AutocompleteProviderTest::ResetControllerWithKeywordProvider() {
  TemplateURLService* turl_model = client_->GetTemplateURLService();

  // Create a TemplateURL for KeywordProvider.
  TemplateURLData data;
  data.SetShortName(u"foo.com");
  data.SetKeyword(u"foo.com");
  data.SetURL("http://foo.com/{searchTerms}");
  data.is_active = TemplateURLData::ActiveStatus::kTrue;
  TemplateURL* keyword_turl =
      turl_model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_NE(0, keyword_turl->id());

  // Make a TemplateURL for KeywordProvider that a shorter version of the
  // first.
  data.SetShortName(u"f");
  data.SetKeyword(u"f");
  data.SetURL("http://f.com/{searchTerms}");
  data.is_active = TemplateURLData::ActiveStatus::kTrue;
  keyword_turl = turl_model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_NE(0, keyword_turl->id());

  // Create another TemplateURL for KeywordProvider.
  data.SetShortName(u"bar.com");
  data.SetKeyword(u"bar.com");
  data.SetURL("http://bar.com/{searchTerms}");
  data.is_active = TemplateURLData::ActiveStatus::kTrue;
  keyword_turl = turl_model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_NE(0, keyword_turl->id());

  ResetControllerWithType(AutocompleteProvider::TYPE_KEYWORD);
}

void AutocompleteProviderTest::ResetControllerWithType(int type) {
  EXPECT_FALSE(client_owned_);
  controller_ = std::make_unique<AutocompleteController>(
      base::WrapUnique(client_.get()), type);
  client_owned_ = true;
}

void AutocompleteProviderTest::RunTest() {
  RunQuery("a", true);
}

void AutocompleteProviderTest::RunKeywordTest(const std::u16string& input,
                                              const KeywordTestData* match_data,
                                              size_t size) {
  ACMatches matches;
  for (size_t i = 0; i < size; ++i) {
    AutocompleteMatch match;
    match.relevance = 1000;  // Arbitrary non-zero value.
    match.allowed_to_be_default_match = true;
    match.fill_into_edit = match_data[i].fill_into_edit;
    match.transition = ui::PAGE_TRANSITION_KEYWORD;
    match.keyword = match_data[i].keyword;
    matches.push_back(match);
  }

  AutocompleteInput autocomplete_input(
      input,
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      TestingSchemeClassifier());
  autocomplete_input.set_prefer_keyword(true);
  controller_->input_ = autocomplete_input;
  AutocompleteResult result;
  result.AppendMatches(matches);
  controller_->UpdateAssociatedKeywords(&result);
  for (size_t j = 0; j < result.size(); ++j) {
    EXPECT_EQ(match_data[j].expected_associated_keyword,
              result.match_at(j)->associated_keyword
                  ? result.match_at(j)->associated_keyword->keyword
                  : std::u16string());
  }
}

void AutocompleteProviderTest::UpdateResultsWithSuggestionGroupsTestData(
    const SuggestionGroupsTestData& test_data) {
  // Create new matches and add to the result.
  size_t relevance = 1000;
  ACMatches matches;
  for (auto suggestion_group_id : test_data.suggestion_group_ids) {
    AutocompleteMatch match(nullptr, relevance--, false,
                            AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED);
    if (suggestion_group_id.has_value()) {
      match.suggestion_group_id = suggestion_group_id.value();
    }
    matches.push_back(match);
  }
  result_.ClearMatches();
  result_.AppendMatches(matches);

  // Update the result with the suggestion groups information.
  result_.MergeSuggestionGroupsMap(test_data.suggestion_groups_map);
  // Group matches with group IDs and move them to the bottom of the result set.
  result_.GroupAndDemoteMatchesInGroups();
}

void AutocompleteProviderTest::RunSearchboxStatsTest(
    const SearchboxStatsTestData* sbs_test_data,
    size_t size,
    bool input_is_zero_suggest) {
  if (input_is_zero_suggest) {
    // Prepare the input.
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestingSchemeClassifier());
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_CLOBBER);
    controller_->input_ = input;
  }

  // Prepare the results.
  const size_t kMaxRelevance = 1000;
  ACMatches matches;
  for (size_t i = 0; i < size; ++i) {
    AutocompleteMatch match(nullptr, kMaxRelevance - i, false,
                            sbs_test_data[i].match_type);
    match.suggestion_group_id = sbs_test_data[i].group_id;
    match.allowed_to_be_default_match = true;
    match.keyword = kTestTemplateURLKeyword;
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(std::u16string());
    match.suggest_type = sbs_test_data[i].type;
    match.subtypes = sbs_test_data[i].subtypes;
    matches.push_back(match);
  }
  result_.Reset();
  result_.AppendMatches(matches);
  result_.MergeSuggestionGroupsMap(omnibox::BuildDefaultGroups());
  result_.set_zero_prefix_enabled_in_session(input_is_zero_suggest);

  // Update Searchbox stats.
  controller_->UpdateSearchboxStats(&result_);

  // Verify data.
  for (size_t i = 0; i < size; ++i) {
    std::string serialized_searchbox_stats;
    result_.match_at(i)->search_terms_args->searchbox_stats.SerializeToString(
        &serialized_searchbox_stats);
    std::string encoded_searchbox_stats;
    base::Base64UrlEncode(serialized_searchbox_stats,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &encoded_searchbox_stats);
    std::string expected_serialized_searchbox_stats;
    sbs_test_data[i].expected_searchbox_stats.SerializeToString(
        &expected_serialized_searchbox_stats);
    std::string expected_encoded_searchbox_stats;
    base::Base64UrlEncode(expected_serialized_searchbox_stats,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,

                          &expected_encoded_searchbox_stats);
    EXPECT_EQ(expected_encoded_searchbox_stats, encoded_searchbox_stats);
  }
}

void AutocompleteProviderTest::RunQuery(const std::string& query,
                                        bool allow_exact_keyword_match) {
  result_.ClearMatches();
  AutocompleteInput input(base::ASCIIToUTF16(query),
                          metrics::OmniboxEventProto::OTHER,
                          TestingSchemeClassifier());
  input.set_prevent_inline_autocomplete(true);
  input.set_allow_exact_keyword_match(allow_exact_keyword_match);

  base::RunLoop run_loop;
  autocomplete_controller_observer_.set_closure(
      run_loop.QuitClosure().Then(base::BindRepeating(
          &AutocompleteProviderTest::CopyResults, base::Unretained(this))));
  if (!autocomplete_controller_observer_.is_observing()) {
    controller_->AddObserver(&autocomplete_controller_observer_);
    autocomplete_controller_observer_.set_is_observing();
  }
  controller_->Start(input);
  if (!controller_->done())
    run_loop.Run();
}

void AutocompleteProviderTest::RunExactKeymatchTest(
    bool allow_exact_keyword_match) {
  // Send the controller input which exactly matches the keyword provider we
  // created in ResetControllerWithKeywordAndSearchProviders().  The default
  // match should thus be a search-other-engine match iff
  // |allow_exact_keyword_match| is true.  Regardless, the match should
  // be from SearchProvider.  (It provides all verbatim search matches,
  // keyword or not.)
  RunQuery("k test", allow_exact_keyword_match);
  EXPECT_EQ(AutocompleteProvider::TYPE_SEARCH,
            controller_->result().default_match()->provider->type());
  EXPECT_EQ(allow_exact_keyword_match
                ? AutocompleteMatchType::SEARCH_OTHER_ENGINE
                : AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            controller_->result().default_match()->type);
}

void AutocompleteProviderTest::CopyResults() {
  result_.CopyMatchesFrom(controller_->result());
}

GURL AutocompleteProviderTest::GetDestinationURL(
    AutocompleteMatch& match,
    base::TimeDelta query_formulation_time) const {
  controller_->UpdateMatchDestinationURLWithAdditionalSearchboxStats(
      query_formulation_time, &match);
  return match.destination_url;
}

// Tests that the default selection is set properly when updating results.
TEST_F(AutocompleteProviderTest, Query) {
  TestProvider* provider1 = nullptr;
  TestProvider* provider2 = nullptr;
  ResetControllerWithTestProviders(false, &provider1, &provider2);
  RunTest();

  // Make sure the default match gets set to the highest relevance match.  The
  // highest relevance matches should come from the second provider.
  EXPECT_EQ(
      std::min(AutocompleteResult::GetMaxMatches(), kResultsPerProvider * 2),
      result_.size());
  ASSERT_TRUE(result_.default_match());
  EXPECT_EQ(provider2, result_.default_match()->provider);
}

// Tests searchbox stats.
TEST_F(AutocompleteProviderTest, SearchboxStats) {
  ResetControllerWithTestProviders(false, nullptr, nullptr);
  RunTest();

  ASSERT_EQ(
      std::min(AutocompleteResult::GetMaxMatches(), kResultsPerProvider * 2),
      result_.size());

  // Now, check the results from the second provider, as they should not have
  // searchbox stats set.
  for (size_t i = 0; i < kResultsPerProvider; ++i) {
    EXPECT_EQ(
        0u,
        result_.match_at(i)->search_terms_args->searchbox_stats.ByteSizeLong());
  }
  // The first provider has a test keyword, so the searchbox stats should be
  // non-empty.
  for (size_t i = kResultsPerProvider; i < result_.size(); ++i) {
    EXPECT_NE(
        0u,
        result_.match_at(i)->search_terms_args->searchbox_stats.ByteSizeLong());
  }
}

TEST_F(AutocompleteProviderTest, RemoveDuplicates) {
  TestProvider* provider1 = nullptr;
  TestProvider* provider2 = nullptr;
  ResetControllerWithTestProviders(true, &provider1, &provider2);
  RunTest();

  // Make sure all the first provider's results were eliminated by the second
  // provider's.
  EXPECT_EQ(kResultsPerProvider, result_.size());
  for (AutocompleteResult::const_iterator i(result_.begin());
       i != result_.end(); ++i)
    EXPECT_EQ(provider2, i->provider);
}

TEST_F(AutocompleteProviderTest, AllowExactKeywordMatch) {
  ResetControllerWithKeywordAndSearchProviders();
  RunExactKeymatchTest(true);
  RunExactKeymatchTest(false);
}

// Ensures matches from (only) the default search provider respect any extra
// query params set on the command line.
TEST_F(AutocompleteProviderTest, ExtraQueryParams) {
  ResetControllerWithKeywordAndSearchProviders();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");
  RunExactKeymatchTest(true);
  CopyResults();

  ASSERT_EQ(1U, result_.size());
  EXPECT_EQ("http://keyword/test",
            result_.match_at(0)->destination_url.possibly_invalid_spec());
}

// Ensures matches from (only) the default search provider are curbed when in
// keyword mode.
TEST_F(AutocompleteProviderTest, CurbDefaultSuggestions) {
  ResetControllerWithKeywordAndSearchProviders();
  RunExactKeymatchTest(true);
  CopyResults();

  // DSE suggestions are curbed in keyword mode, so the default turl suggestion
  // should not be present in the res=ults.
  ASSERT_EQ(1U, result_.size());
  EXPECT_EQ("http://keyword/test",
            result_.match_at(0)->destination_url.possibly_invalid_spec());
}

// Test that redundant associated keywords are removed.
TEST_F(AutocompleteProviderTest, RedundantKeywordsIgnoredInResult) {
  ResetControllerWithKeywordProvider();

  {
    KeywordTestData duplicate_url[] = {
        {u"fo", std::u16string(), std::u16string()},
        {u"foo.com", std::u16string(), u"foo.com"},
        {u"foo.com", std::u16string(), std::u16string()}};

    SCOPED_TRACE("Duplicate url");
    RunKeywordTest(u"fo", duplicate_url, std::size(duplicate_url));
  }

  {
    KeywordTestData keyword_match[] = {
        {u"foo.com", u"foo.com", std::u16string()},
        {u"foo.com", std::u16string(), std::u16string()}};

    SCOPED_TRACE("Duplicate url with keyword match");
    RunKeywordTest(u"fo", keyword_match, std::size(keyword_match));
  }

  {
    KeywordTestData multiple_keyword[] = {
        {u"fo", std::u16string(), std::u16string()},
        {u"foo.com", std::u16string(), u"foo.com"},
        {u"foo.com", std::u16string(), std::u16string()},
        {u"bar.com", std::u16string(), u"bar.com"},
    };

    SCOPED_TRACE("Duplicate url with multiple keywords");
    RunKeywordTest(u"fo", multiple_keyword, std::size(multiple_keyword));
  }
}

// Test that exact match keywords trump keywords associated with
// the match.
TEST_F(AutocompleteProviderTest, ExactMatchKeywords) {
  ResetControllerWithKeywordProvider();

  {
    KeywordTestData keyword_match[] = {
        {u"foo.com", std::u16string(), u"foo.com"}};

    SCOPED_TRACE("keyword match as usual");
    RunKeywordTest(u"fo", keyword_match, std::size(keyword_match));
  }

  // The same result set with an input of "f" (versus "fo") should get
  // a different associated keyword because "f" is an exact match for
  // a keyword and that should trump the keyword normally associated with
  // this match.
  {
    KeywordTestData keyword_match[] = {{u"foo.com", std::u16string(), u"f"}};

    SCOPED_TRACE("keyword exact match");
    RunKeywordTest(u"f", keyword_match, std::size(keyword_match));
  }
}

// Tests that the AutocompleteResult is updated with the suggestion group
// information and matches with group IDs are grouped and demoted correctly.
// Also verifies that:
// 1) headers are optional for suggestion groups.
// 2) suggestion groups are ordered based on their priories.
// 3) suggestion group IDs without associated suggestion group information are
//    stripped away.
TEST_F(AutocompleteProviderTest, SuggestionGroups) {
  ResetControllerWithKeywordAndSearchProviders();

  const auto kRecommendedGroupId = omnibox::GROUP_PREVIOUS_SEARCH_RELATED;
  const std::string kRecommended = "Recommended for you";
  const auto kRecentSearchesGroupId =
      omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS;
  const std::string kRecentSearches = "Recent Searches";

  const auto kBadGroupId = omnibox::GROUP_INVALID;

  {
    // AutocompleteResult::GetHeaderForSuggestionGroup() returns an empty string
    // for unknown suggestion group IDs.
    EXPECT_EQ(u"", result_.GetHeaderForSuggestionGroup(kBadGroupId));

    // AutocompleteResult::IsSuggestionGroupHidden() returns false for unknown
    // suggestion group IDs.
    EXPECT_FALSE(result_.IsSuggestionGroupHidden(GetPrefs(), kBadGroupId));

    // AutocompleteResult::SetSuggestionGroupHidden() does nothing for unknown
    // suggestion group IDs.
    result_.SetSuggestionGroupHidden(GetPrefs(), kBadGroupId,
                                     /*hidden=*/true);
    EXPECT_FALSE(result_.IsSuggestionGroupHidden(GetPrefs(), kBadGroupId));

    // AutocompleteResult::GetSectionForSuggestionGroup() returns
    // omnibox::SECTION_DEFAULT for unknown suggestion group IDs.
    EXPECT_EQ(omnibox::SECTION_DEFAULT,
              result_.GetSectionForSuggestionGroup(kBadGroupId));
  }
  {
    // Headers are optional for suggestion groups.
    omnibox::GroupConfigMap suggestion_groups_map;
    suggestion_groups_map[kRecommendedGroupId].set_header_text(kRecommended);
    suggestion_groups_map[kRecommendedGroupId].set_section(
        omnibox::SECTION_REMOTE_ZPS_4);

    suggestion_groups_map[kRecentSearchesGroupId].set_section(
        omnibox::SECTION_REMOTE_ZPS_3);

    UpdateResultsWithSuggestionGroupsTestData({std::move(suggestion_groups_map),
                                               {
                                                   {},
                                                   {},
                                                   {},
                                                   {kRecentSearchesGroupId},
                                                   {kRecommendedGroupId},
                                               }});

    EXPECT_FALSE(result_.match_at(0)->suggestion_group_id.has_value());

    EXPECT_FALSE(result_.match_at(1)->suggestion_group_id.has_value());

    EXPECT_FALSE(result_.match_at(2)->suggestion_group_id.has_value());

    EXPECT_EQ(kRecentSearchesGroupId,
              result_.match_at(3)->suggestion_group_id.value());
    EXPECT_EQ(u"", result_.GetHeaderForSuggestionGroup(kRecentSearchesGroupId));

    EXPECT_EQ(kRecommendedGroupId,
              result_.match_at(4)->suggestion_group_id.value());
    EXPECT_EQ(base::UTF8ToUTF16(kRecommended),
              result_.GetHeaderForSuggestionGroup(kRecommendedGroupId));
  }
  {
    // Suggestion groups are ordered based on their priories.
    omnibox::GroupConfigMap suggestion_groups_map;
    suggestion_groups_map[kRecommendedGroupId].set_header_text(kRecommended);
    suggestion_groups_map[kRecommendedGroupId].set_section(
        omnibox::SECTION_REMOTE_ZPS_3);
    suggestion_groups_map[kRecentSearchesGroupId].set_header_text(
        kRecentSearches);
    suggestion_groups_map[kRecentSearchesGroupId].set_section(
        omnibox::SECTION_REMOTE_ZPS_4);
    UpdateResultsWithSuggestionGroupsTestData({std::move(suggestion_groups_map),
                                               {
                                                   {},
                                                   {kRecentSearchesGroupId},
                                                   {},
                                                   {kRecommendedGroupId},
                                                   {kRecentSearchesGroupId},
                                               }});

    EXPECT_FALSE(result_.match_at(0)->suggestion_group_id.has_value());

    EXPECT_FALSE(result_.match_at(1)->suggestion_group_id.has_value());

    EXPECT_EQ(kRecommendedGroupId,
              result_.match_at(2)->suggestion_group_id.value());
    EXPECT_EQ(base::UTF8ToUTF16(kRecommended),
              result_.GetHeaderForSuggestionGroup(kRecommendedGroupId));

    EXPECT_EQ(kRecentSearchesGroupId,
              result_.match_at(3)->suggestion_group_id.value());
    EXPECT_EQ(base::UTF8ToUTF16(kRecentSearches),
              result_.GetHeaderForSuggestionGroup(kRecentSearchesGroupId));

    EXPECT_EQ(kRecentSearchesGroupId,
              result_.match_at(4)->suggestion_group_id.value());
    EXPECT_EQ(base::UTF8ToUTF16(kRecentSearches),
              result_.GetHeaderForSuggestionGroup(kRecentSearchesGroupId));
  }
  {
    // suggestion group IDs without associated suggestion group information are
    // stripped away.
    omnibox::GroupConfigMap suggestion_groups_map;
    suggestion_groups_map[kRecommendedGroupId].set_header_text(kRecommended);
    suggestion_groups_map[kRecentSearchesGroupId].set_header_text(
        kRecentSearches);
    UpdateResultsWithSuggestionGroupsTestData({std::move(suggestion_groups_map),
                                               {
                                                   {kBadGroupId},
                                                   {kRecentSearchesGroupId},
                                                   {kRecommendedGroupId},
                                                   {kBadGroupId},
                                                   {kBadGroupId},
                                               }});

    EXPECT_FALSE(result_.match_at(0)->suggestion_group_id.has_value());

    EXPECT_FALSE(result_.match_at(1)->suggestion_group_id.has_value());

    EXPECT_FALSE(result_.match_at(2)->suggestion_group_id.has_value());

    EXPECT_EQ(kRecentSearchesGroupId,
              result_.match_at(3)->suggestion_group_id.value());
    EXPECT_EQ(base::UTF8ToUTF16(kRecentSearches),
              result_.GetHeaderForSuggestionGroup(kRecentSearchesGroupId));

    EXPECT_EQ(kRecommendedGroupId,
              result_.match_at(4)->suggestion_group_id.value());
    EXPECT_EQ(base::UTF8ToUTF16(kRecommended),
              result_.GetHeaderForSuggestionGroup(kRecommendedGroupId));
  }
}

TEST_F(AutocompleteProviderTest, UpdateSearchboxStats) {
  ResetControllerWithTestProviders(false, nullptr, nullptr);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kCategoricalSuggestions);

  {
    omnibox::metrics::ChromeSearchboxStats searchbox_stats;
    SearchboxStatsTestData test_data[] = {
        //  MSVC doesn't support zero-length arrays, so supply some dummy data.
        {AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
         {/* GroupID */},
         searchbox_stats,
         omnibox::TYPE_NATIVE_CHROME}};
    SCOPED_TRACE("No matches");
    // Note: We pass 0 here to ignore the dummy data above.
    RunSearchboxStatsTest(test_data, 0, /*input_is_zero_suggest=*/false);
  }

  // Note: See suggest.proto for the types and subtypes referenced below.

  {
    omnibox::metrics::ChromeSearchboxStats searchbox_stats;
    searchbox_stats.set_client_name("chrome");
    searchbox_stats.set_num_zero_prefix_suggestions_shown(0);
    searchbox_stats.set_zero_prefix_enabled(false);
    auto* available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(0);
    available_suggestion->set_type(omnibox::TYPE_NATIVE_CHROME);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_OMNIBOX_ECHO_SEARCH);

    SearchboxStatsTestData test_data[] = {
        {AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
         {/* GroupID */},
         searchbox_stats,
         omnibox::TYPE_NATIVE_CHROME}};
    SCOPED_TRACE("One match");
    RunSearchboxStatsTest(test_data, std::size(test_data),
                          /*input_is_zero_suggest=*/false);
  }

  {
    omnibox::metrics::ChromeSearchboxStats searchbox_stats;
    searchbox_stats.set_client_name("chrome");
    searchbox_stats.set_num_zero_prefix_suggestions_shown(0);
    searchbox_stats.set_zero_prefix_enabled(false);
    auto* available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(0);
    available_suggestion->set_type(omnibox::TYPE_ENTITY);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_PERSONAL);
    auto* assisted_query_info = searchbox_stats.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(0));

    SearchboxStatsTestData test_data[] = {
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         {/* GroupID */},
         searchbox_stats,
         omnibox::TYPE_ENTITY,
         {omnibox::SUBTYPE_PERSONAL}}};
    SCOPED_TRACE("One match with provider populated subtypes");
    RunSearchboxStatsTest(test_data, std::size(test_data),
                          /*input_is_zero_suggest=*/false);
  }

  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(
        omnibox::kMostVisitedTilesHorizontalRenderGroup);
    omnibox::ResetDefaultGroupsForTest();

    omnibox::metrics::ChromeSearchboxStats searchbox_stats;
    searchbox_stats.set_client_name("chrome");
    searchbox_stats.set_num_zero_prefix_suggestions_shown(1);
    searchbox_stats.set_zero_prefix_enabled(true);
    auto* available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(0);
    available_suggestion->set_type(omnibox::TYPE_QUERY);
    available_suggestion->add_subtypes(
        omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_QUERIES);
    auto* assisted_query_info = searchbox_stats.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(0));

    SearchboxStatsTestData test_data[] = {
        {AutocompleteMatchType::SEARCH_SUGGEST,
         {omnibox::GROUP_MOBILE_MOST_VISITED},
         searchbox_stats,
         omnibox::TYPE_QUERY,
         {omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_QUERIES}},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         {omnibox::GROUP_MOBILE_MOST_VISITED},
         searchbox_stats,
         omnibox::TYPE_QUERY,
         {omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_QUERIES}},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         {omnibox::GROUP_MOBILE_MOST_VISITED},
         searchbox_stats,
         omnibox::TYPE_QUERY,
         {omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_QUERIES}},
    };
    SCOPED_TRACE("Multiple matches in horizontal render group");
    RunSearchboxStatsTest(test_data, std::size(test_data),
                          /*input_is_zero_suggest=*/true);
  }

  {
    base::test::ScopedFeatureList features;
    features.InitAndEnableFeature(
        omnibox::kMostVisitedTilesHorizontalRenderGroup);
    omnibox::ResetDefaultGroupsForTest();

    omnibox::metrics::ChromeSearchboxStats searchbox_stats;
    searchbox_stats.set_client_name("chrome");
    searchbox_stats.set_num_zero_prefix_suggestions_shown(3);
    searchbox_stats.set_zero_prefix_enabled(true);

    auto* available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(0);
    available_suggestion->set_type(omnibox::TYPE_ENTITY);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_ZERO_PREFIX);

    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(1);
    available_suggestion->set_type(omnibox::TYPE_QUERY);
    available_suggestion->add_subtypes(
        omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_QUERIES);

    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(2);
    available_suggestion->set_type(omnibox::TYPE_ENTITY);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_ZERO_PREFIX);

    auto stats_0 = searchbox_stats;
    auto* assisted_query_info = stats_0.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(0));

    auto stats_1 = searchbox_stats;
    assisted_query_info = stats_1.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(1));

    auto stats_2 = searchbox_stats;
    assisted_query_info = stats_2.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(2));

    SearchboxStatsTestData test_data[] = {
        // Entity Suggestion
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         {/* GroupID */},
         stats_0,
         omnibox::TYPE_ENTITY,
         {omnibox::SUBTYPE_ZERO_PREFIX}},
        // Three horizontally rendered tiles
        {AutocompleteMatchType::SEARCH_SUGGEST,
         {omnibox::GROUP_MOBILE_MOST_VISITED},
         stats_1,
         omnibox::TYPE_QUERY,
         {omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_QUERIES}},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         {omnibox::GROUP_MOBILE_MOST_VISITED},
         stats_1,
         omnibox::TYPE_QUERY,
         {omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS}},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         {omnibox::GROUP_MOBILE_MOST_VISITED},
         stats_1,
         omnibox::TYPE_QUERY,
         {omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_HISTORY}},
        // Entity suggestion.
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         {/* GroupID */},
         stats_2,
         omnibox::TYPE_ENTITY,
         {omnibox::SUBTYPE_ZERO_PREFIX}},
    };
    SCOPED_TRACE("Multiple matches with horizontal render group");
    RunSearchboxStatsTest(test_data, std::size(test_data),
                          /*input_is_zero_suggest=*/true);
  }

  {
    omnibox::metrics::ChromeSearchboxStats searchbox_stats;
    searchbox_stats.set_client_name("chrome");
    searchbox_stats.set_num_zero_prefix_suggestions_shown(2);
    searchbox_stats.set_zero_prefix_enabled(true);
    auto* available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(0);
    available_suggestion->set_type(omnibox::TYPE_QUERY);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_PERSONAL);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_TRENDS);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_ZERO_PREFIX);
    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(1);
    available_suggestion->set_type(omnibox::TYPE_ENTITY);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_PERSONAL);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_TRENDS);
    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(2);
    available_suggestion->set_type(omnibox::TYPE_CATEGORICAL_QUERY);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_PERSONAL);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_TRENDS);
    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(3);
    available_suggestion->set_type(omnibox::TYPE_ENTITY);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_PERSONAL);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_TRENDS);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_ZERO_PREFIX);
    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(4);
    available_suggestion->set_type(omnibox::TYPE_ENTITY);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_PERSONAL);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_TRENDS);

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_0;
    searchbox_stats_0.MergeFrom(searchbox_stats);
    auto* assisted_query_info = searchbox_stats_0.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(0));

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_1;
    searchbox_stats_1.MergeFrom(searchbox_stats);
    assisted_query_info = searchbox_stats_1.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(1));

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_2;
    searchbox_stats_2.MergeFrom(searchbox_stats);
    assisted_query_info = searchbox_stats_2.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(2));

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_3;
    searchbox_stats_3.MergeFrom(searchbox_stats);
    assisted_query_info = searchbox_stats_3.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(3));

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_4;
    searchbox_stats_4.MergeFrom(searchbox_stats);
    assisted_query_info = searchbox_stats_4.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(4));

    // This test confirms that repetitive subtype information is being
    // properly handled and reported as the same suggestion type.
    SearchboxStatsTestData test_data[] = {
        {AutocompleteMatchType::SEARCH_SUGGEST,
         {/* GroupID */},
         searchbox_stats_0,
         omnibox::TYPE_QUERY,
         {omnibox::SUBTYPE_PERSONAL, omnibox::SUBTYPE_TRENDS,
          omnibox::SUBTYPE_ZERO_PREFIX, omnibox::SUBTYPE_TRENDS}},
        // The next two matches should be detected as the same type, despite
        // repeated subtype match.
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         {/* GroupID */},
         searchbox_stats_1,
         omnibox::TYPE_ENTITY,
         {omnibox::SUBTYPE_PERSONAL, omnibox::SUBTYPE_TRENDS}},
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         {/* GroupID */},
         searchbox_stats_2,
         omnibox::TYPE_CATEGORICAL_QUERY,
         {omnibox::SUBTYPE_PERSONAL, omnibox::SUBTYPE_TRENDS,
          omnibox::SUBTYPE_PERSONAL}},
        // This match should not be bundled together with previous two, because
        // it comes with additional subtype information (42).
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         {/* GroupID */},
         searchbox_stats_3,
         omnibox::TYPE_ENTITY,
         {omnibox::SUBTYPE_PERSONAL, omnibox::SUBTYPE_TRENDS,
          omnibox::SUBTYPE_ZERO_PREFIX}},
        // This match should not be bundled together with the group before,
        // because these items are not adjacent.
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         {/* GroupID */},
         searchbox_stats_4,
         omnibox::TYPE_ENTITY,
         {omnibox::SUBTYPE_PERSONAL, omnibox::SUBTYPE_TRENDS}},
    };
    SCOPED_TRACE("Complex set of matches with repetitive subtypes");
    RunSearchboxStatsTest(test_data, std::size(test_data),
                          /*input_is_zero_suggest=*/true);
  }

  // This test confirms that selection of trivial suggestions does not get
  // reported in `assisted_query_info`. And that the count of zero-prefix
  // matches coming from the suggest server or the local device are recorded.
  {
    omnibox::metrics::ChromeSearchboxStats searchbox_stats;
    searchbox_stats.set_client_name("chrome");
    searchbox_stats.set_num_zero_prefix_suggestions_shown(3);
    searchbox_stats.set_zero_prefix_enabled(true);
    auto* available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(0);
    available_suggestion->set_type(omnibox::TYPE_NATIVE_CHROME);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_OMNIBOX_ECHO_SEARCH);
    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(1);
    available_suggestion->set_type(omnibox::TYPE_NATIVE_CHROME);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_OMNIBOX_ECHO_URL);
    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(2);
    available_suggestion->set_type(omnibox::TYPE_NAVIGATION);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_OMNIBOX_OTHER);
    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(3);
    available_suggestion->set_type(omnibox::TYPE_NAVIGATION);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_OMNIBOX_OTHER);
    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(4);
    available_suggestion->set_type(omnibox::TYPE_QUERY);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_ZERO_PREFIX);
    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(5);
    available_suggestion->set_type(omnibox::TYPE_QUERY);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_ZERO_PREFIX);
    available_suggestion->add_subtypes(
        omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_HISTORY);
    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(6);
    available_suggestion->set_type(omnibox::TYPE_QUERY);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_ZERO_PREFIX);
    available_suggestion->add_subtypes(
        omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS);
    available_suggestion = searchbox_stats.add_available_suggestions();
    available_suggestion->set_index(7);
    available_suggestion->set_type(omnibox::TYPE_NATIVE_CHROME);
    available_suggestion->add_subtypes(omnibox::SUBTYPE_OMNIBOX_HISTORY_SEARCH);

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_0;
    searchbox_stats_0.MergeFrom(searchbox_stats);

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_1;
    searchbox_stats_1.MergeFrom(searchbox_stats);

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_2;
    searchbox_stats_2.MergeFrom(searchbox_stats);
    auto* assisted_query_info = searchbox_stats_2.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(2));

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_3;
    searchbox_stats_3.MergeFrom(searchbox_stats);
    assisted_query_info = searchbox_stats_3.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(3));

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_4;
    searchbox_stats_4.MergeFrom(searchbox_stats);
    assisted_query_info = searchbox_stats_4.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(4));

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_5;
    searchbox_stats_5.MergeFrom(searchbox_stats);
    assisted_query_info = searchbox_stats_5.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(5));

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_6;
    searchbox_stats_6.MergeFrom(searchbox_stats);
    assisted_query_info = searchbox_stats_6.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(6));

    omnibox::metrics::ChromeSearchboxStats searchbox_stats_7;
    searchbox_stats_7.MergeFrom(searchbox_stats);
    assisted_query_info = searchbox_stats_7.mutable_assisted_query_info();
    assisted_query_info->MergeFrom(searchbox_stats.available_suggestions(7));

    SearchboxStatsTestData test_data[] = {
        {AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
         {/* GroupID */},
         searchbox_stats_0,
         omnibox::TYPE_NATIVE_CHROME},
        {AutocompleteMatchType::URL_WHAT_YOU_TYPED,
         {/* GroupID */},
         searchbox_stats_1,
         omnibox::TYPE_NATIVE_CHROME},
        {AutocompleteMatchType::NAVSUGGEST,
         {/* GroupID */},
         searchbox_stats_2,
         omnibox::TYPE_NAVIGATION},
        {AutocompleteMatchType::NAVSUGGEST,
         {/* GroupID */},
         searchbox_stats_3,
         omnibox::TYPE_NAVIGATION},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         {/* GroupID */},
         searchbox_stats_4,
         omnibox::TYPE_QUERY,
         {omnibox::SUBTYPE_ZERO_PREFIX}},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         {/* GroupID */},
         searchbox_stats_5,
         omnibox::TYPE_QUERY,
         {omnibox::SUBTYPE_ZERO_PREFIX,
          omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_HISTORY}},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         {/* GroupID */},
         searchbox_stats_6,
         omnibox::TYPE_QUERY,
         {omnibox::SUBTYPE_ZERO_PREFIX,
          omnibox::SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URLS}},
        {AutocompleteMatchType::SEARCH_HISTORY,
         {/* GroupID */},
         searchbox_stats_7,
         omnibox::TYPE_NATIVE_CHROME},
    };
    SCOPED_TRACE("Trivial and zero-prefix matches");
    RunSearchboxStatsTest(test_data, std::size(test_data),
                          /*input_is_zero_suggest=*/true);
  }
}

TEST_F(AutocompleteProviderTest, GetDestinationURL) {
  ResetControllerWithKeywordAndSearchProviders();

  // For the destination URL to have searchbox stats parameters for query
  // formulation time and the field trial triggered bit, many conditions need
  // to be satisfied.
  AutocompleteMatch match(nullptr, 1100, false,
                          AutocompleteMatchType::SEARCH_SUGGEST);
  GURL url(GetDestinationURL(match, base::Milliseconds(2456)));
  EXPECT_TRUE(url.path().empty());

  // The protocol needs to be https.
  RegisterTemplateURL(kTestTemplateURLKeyword,
                      "https://foo/{searchTerms}/{google:assistedQueryStats}");
  url = GetDestinationURL(match, base::Milliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // There needs to be a keyword provider.
  match.keyword = kTestTemplateURLKeyword;
  url = GetDestinationURL(match, base::Milliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // search_terms_args needs to be set.
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(std::u16string());
  url = GetDestinationURL(match, base::Milliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // searchbox_stats need to have been set.
  match.search_terms_args->searchbox_stats.set_client_name("chrome");
  url = GetDestinationURL(match, base::Milliseconds(2456));
  EXPECT_EQ("//gs_lcrp=EgZjaHJvbWXSAQgyNDU2ajBqMA&", url.path());
  // Make sure searchbox_stats is serialized and encoded correctly.
  {
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        "EgZjaHJvbWXSAQgyNDU2ajBqMA",
        base::Base64UrlDecodePolicy::DISALLOW_PADDING, &serialized_proto));
    omnibox::metrics::ChromeSearchboxStats expected;
    expected.ParseFromString(serialized_proto);
    EXPECT_EQ("chrome", expected.client_name());
  }

  // Test field trial triggered bit set.
  set_remote_search_feature_triggered_in_session(true);
  url = GetDestinationURL(match, base::Milliseconds(2456));
  EXPECT_EQ("//gs_lcrp=EgZjaHJvbWXSAQgyNDU2ajFqMA&", url.path());
  // Make sure searchbox_stats is serialized and encoded correctly.
  {
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        "EgZjaHJvbWXSAQgyNDU2ajFqMA",
        base::Base64UrlDecodePolicy::DISALLOW_PADDING, &serialized_proto));
    omnibox::metrics::ChromeSearchboxStats expected;
    expected.ParseFromString(serialized_proto);
    EXPECT_EQ("2456j1j0", expected.experiment_stats());
  }

  // Test page classification set.
  set_remote_search_feature_triggered_in_session(false);
  set_current_page_classification(metrics::OmniboxEventProto::OTHER);
  url = GetDestinationURL(match, base::Milliseconds(2456));
  EXPECT_EQ("//gs_lcrp=EgZjaHJvbWXSAQgyNDU2ajBqNA&", url.path());
  // Make sure searchbox_stats is serialized and encoded correctly.
  {
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        "EgZjaHJvbWXSAQgyNDU2ajBqNA",
        base::Base64UrlDecodePolicy::DISALLOW_PADDING, &serialized_proto));
    omnibox::metrics::ChromeSearchboxStats expected;
    expected.ParseFromString(serialized_proto);
    EXPECT_EQ("2456j0j4", expected.experiment_stats());
  }

  // Test page classification and field trial triggered set.
  set_remote_search_feature_triggered_in_session(true);
  set_current_page_classification(metrics::OmniboxEventProto::OTHER);
  url = GetDestinationURL(match, base::Milliseconds(2456));
  EXPECT_EQ("//gs_lcrp=EgZjaHJvbWXSAQgyNDU2ajFqNA&", url.path());
  // Make sure searchbox_stats is serialized and encoded correctly.
  {
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        "EgZjaHJvbWXSAQgyNDU2ajFqNA",
        base::Base64UrlDecodePolicy::DISALLOW_PADDING, &serialized_proto));
    omnibox::metrics::ChromeSearchboxStats expected;
    expected.ParseFromString(serialized_proto);
    EXPECT_EQ("2456j1j4", expected.experiment_stats());
  }

#if BUILDFLAG(IS_IOS)
  {  // Test top omnibox position in experiment stats v2.
    AutocompleteMatch match_copy = match;
    controller_->SetSteadyStateOmniboxPosition(
        metrics::OmniboxEventProto::TOP_POSITION);
    url = GetDestinationURL(match_copy, base::Milliseconds(2456));
    EXPECT_EQ("//gs_lcrp=EgZjaHJvbWXSAQgyNDU2ajFqNOIDBBgBIF8&", url.path());
    // Make sure searchbox_stats is serialized and encoded correctly.
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        "EgZjaHJvbWXSAQgyNDU2ajFqNOIDBBgBIF8",
        base::Base64UrlDecodePolicy::DISALLOW_PADDING, &serialized_proto));
    omnibox::metrics::ChromeSearchboxStats expected;
    expected.ParseFromString(serialized_proto);
    EXPECT_EQ(1, expected.experiment_stats_v2_size());
    EXPECT_EQ(95, expected.experiment_stats_v2(0).type_int());
    EXPECT_EQ(1, expected.experiment_stats_v2(0).int_value());
  }
  {  // Test bottom omnibox position in experiment stats v2.
    AutocompleteMatch match_copy = match;
    controller_->SetSteadyStateOmniboxPosition(
        metrics::OmniboxEventProto::BOTTOM_POSITION);
    url = GetDestinationURL(match_copy, base::Milliseconds(2456));
    EXPECT_EQ("//gs_lcrp=EgZjaHJvbWXSAQgyNDU2ajFqNOIDBBgCIF8&", url.path());
    // Make sure searchbox_stats is serialized and encoded correctly.
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        "EgZjaHJvbWXSAQgyNDU2ajFqNOIDBBgCIF8",
        base::Base64UrlDecodePolicy::DISALLOW_PADDING, &serialized_proto));
    omnibox::metrics::ChromeSearchboxStats expected;
    expected.ParseFromString(serialized_proto);
    EXPECT_EQ(1, expected.experiment_stats_v2_size());
    EXPECT_EQ(95, expected.experiment_stats_v2(0).type_int());
    EXPECT_EQ(2, expected.experiment_stats_v2(0).int_value());
  }
  controller_->SetSteadyStateOmniboxPosition(
      metrics::OmniboxEventProto::UNKNOWN_POSITION);
#endif

  // Test experiment stats v2 set.
  omnibox::metrics::ChromeSearchboxStats::ExperimentStatsV2 experiment_stats_v2;
  experiment_stats_v2.set_type_int(10001);
  experiment_stats_v2.set_string_value("0:67");
  add_zero_suggest_provider_experiment_stats_v2(experiment_stats_v2);
  url = GetDestinationURL(match, base::Milliseconds(2456));
  EXPECT_EQ("//gs_lcrp=EgZjaHJvbWXSAQgyNDU2ajFqNOIDCRIEMCw2NyCRTg&",
            url.path());
  // Make sure searchbox_stats is serialized and encoded correctly.
  {
    std::string serialized_proto;
    EXPECT_TRUE(base::Base64UrlDecode(
        "EgZjaHJvbWXSAQgyNDU2ajFqNOIDCRIEMCw2NyCRTg",
        base::Base64UrlDecodePolicy::DISALLOW_PADDING, &serialized_proto));
    omnibox::metrics::ChromeSearchboxStats expected;
    expected.ParseFromString(serialized_proto);
    EXPECT_EQ(1, expected.experiment_stats_v2_size());
    EXPECT_EQ(10001, expected.experiment_stats_v2(0).type_int());
    EXPECT_EQ("0,67", expected.experiment_stats_v2(0).string_value());
  }
}

TEST_F(AutocompleteProviderTest, ClassifyAllMatchesInString) {
  ResetControllerWithKeywordAndSearchProviders();

  ACMatchClassifications matches =
      AutocompleteMatch::ClassificationsFromString("0,0");
  ClassifyTest classify_test(u"A man, a plan, a canal Panama",
                             /*text_is_query=*/false, matches);

  ACMatchClassifications spans;

  spans = classify_test.RunTest(u"man");
  //                           A man, a plan, a canal Panama
  // ACMatch spans should be: '--MMM------------------------'
  EXPECT_EQ("0,0,2,2,5,0", AutocompleteMatch::ClassificationsToString(spans));

  spans = classify_test.RunTest(u"man p");
  //                           A man, a plan, a canal Panama
  // ACMatch spans should be: '--MMM----M-------------M-----'
  EXPECT_EQ("0,0,2,2,5,0,9,2,10,0,23,2,24,0",
            AutocompleteMatch::ClassificationsToString(spans));

  // Comparisons should be case insensitive.
  spans = classify_test.RunTest(u"mAn pLAn panAMa");
  //                           A man, a plan, a canal Panama
  // ACMatch spans should be: '--MMM----MMMM----------MMMMMM'
  EXPECT_EQ("0,0,2,2,5,0,9,2,13,0,23,2",
            AutocompleteMatch::ClassificationsToString(spans));

  // When the user input is a prefix of the suggest text, subsequent occurrences
  // of the user words should be matched.
  spans = classify_test.RunTest(u"a man,");
  //                           A man, a plan, a canal Panama
  // ACMatch spans should be: 'MMMMMM-----------------------'
  EXPECT_EQ("0,2,6,0", AutocompleteMatch::ClassificationsToString(spans));

  // Matches must begin at word-starts in the suggest text. E.g. 'a' in 'plan'
  // should not match.
  spans = classify_test.RunTest(u"a man, p");
  //                           A man, a plan, a canal Panama
  // ACMatch spans should be: 'M-MMM--M-M-----M-------M-----'
  EXPECT_EQ("0,2,1,0,2,2,5,0,7,2,8,0,9,2,10,0,15,2,16,0,23,2,24,0",
            AutocompleteMatch::ClassificationsToString(spans));

  ClassifyTest classify_test2(
      u"Yahoo! Sports - Sports News, "
      u"Scores, Rumors, Fantasy Games, and more",
      /*text_is_query=*/false, matches);

  spans = classify_test2.RunTest(u"ne");
  // Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more
  // -----------------------MM-------------------------------------------
  EXPECT_EQ("0,0,23,2,25,0", AutocompleteMatch::ClassificationsToString(spans));

  spans = classify_test2.RunTest(u"neWs R");
  // Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more
  // -----------------------MMMM----------M------------------------------
  EXPECT_EQ("0,0,23,2,27,0,37,2,38,0",
            AutocompleteMatch::ClassificationsToString(spans));

  matches = AutocompleteMatch::ClassificationsFromString("0,1");
  ClassifyTest classify_test3(u"livescore.goal.com",
                              /*text_is_query=*/false, matches);

  // Matches should be merged with existing classifications.
  // Matches can begin after symbols in the suggest text.
  spans = classify_test3.RunTest(u"go");
  // livescore.goal.com
  // ----------MM------
  // ACMatch spans should match first two letters of the "goal".
  EXPECT_EQ("0,1,10,3,12,1", AutocompleteMatch::ClassificationsToString(spans));

  matches = AutocompleteMatch::ClassificationsFromString("0,0,13,1");
  ClassifyTest classify_test4(u"Email login: mail.somecorp.com",
                              /*text_is_query=*/false, matches);

  // Matches must begin at word-starts in the suggest text.
  spans = classify_test4.RunTest(u"ail");
  // Email login: mail.somecorp.com
  // 000000000000011111111111111111
  EXPECT_EQ("0,0,13,1", AutocompleteMatch::ClassificationsToString(spans));

  // The longest matches should take precedence (e.g. 'log' instead of 'lo').
  spans = classify_test4.RunTest(u"lo log mail em");
  // Email login: mail.somecorp.com
  // 220000222000033331111111111111
  EXPECT_EQ("0,2,2,0,6,2,9,0,13,3,17,1",
            AutocompleteMatch::ClassificationsToString(spans));

  // Some web sites do not have a description.  If the string being searched is
  // empty, the classifications must also be empty: http://crbug.com/148647
  // Extra parens in the next line hack around C++03's "most vexing parse".
  class ClassifyTest classify_test5((std::u16string()), /*text_is_query=*/false,
                                    ACMatchClassifications());
  spans = classify_test5.RunTest(u"man");
  ASSERT_EQ(0U, spans.size());

  // Matches which end at beginning of classification merge properly.
  matches = AutocompleteMatch::ClassificationsFromString("0,4,9,0");
  ClassifyTest classify_test6(u"html password example",
                              /*text_is_query=*/false, matches);

  // Extra space in the next string avoids having the string be a prefix of the
  // text above, which would allow for two different valid classification sets,
  // one of which uses two spans (the first of which would mark all of "html
  // pass" as a match) and one which uses four (which marks the individual words
  // as matches but not the space between them).  This way only the latter is
  // valid.
  spans = classify_test6.RunTest(u"html  pass");
  EXPECT_EQ("0,6,4,4,5,6,9,0",
            AutocompleteMatch::ClassificationsToString(spans));

  // Multiple matches with both beginning and end at beginning of
  // classifications merge properly.
  matches = AutocompleteMatch::ClassificationsFromString("0,1,11,0");
  ClassifyTest classify_test7(u"http://a.co is great",
                              /*text_is_query=*/false, matches);

  spans = classify_test7.RunTest(u"ht co");
  EXPECT_EQ("0,3,2,1,9,3,11,0",
            AutocompleteMatch::ClassificationsToString(spans));

  // Search queries should be bold non-matches and unbold matches.
  matches = AutocompleteMatch::ClassificationsFromString("0,0");
  ClassifyTest classify_test8(u"panama canal",
                              /*text_is_query=*/true, matches);

  spans = classify_test8.RunTest(u"pan");
  //                           panama canal
  // ACMatch spans should be: "---MMMMMMMMM";
  EXPECT_EQ("0,0,3,2", AutocompleteMatch::ClassificationsToString(spans));
  spans = classify_test8.RunTest(u"canal");
  //                           panama canal
  // ACMatch spans should be: "MMMMMMM-----";
  EXPECT_EQ("0,2,7,0", AutocompleteMatch::ClassificationsToString(spans));

  // Search autocomplete suggestion.
  ClassifyTest classify_test9(u"comcast webmail login",
                              /*text_is_query=*/true, ACMatchClassifications());

  // Matches first and first part of middle word and the last word.
  spans = classify_test9.RunTest(u"comcast web login");
  //                           comcast webmail login
  // ACMatch spans should be: "-------M---MMMMM-----";
  EXPECT_EQ("0,0,7,2,8,0,11,2,16,0",
            AutocompleteMatch::ClassificationsToString(spans));

  // Matches partial word in the middle of suggestion.
  spans = classify_test9.RunTest(u"web");
  //                           comcast webmail login
  // ACMatch spans should be: "MMMMMMMM---MMMMMMMMMM";
  EXPECT_EQ("0,2,8,0,11,2", AutocompleteMatch::ClassificationsToString(spans));

  ClassifyTest classify_test10(u"comcast.net web mail login",
                               /*text_is_query=*/true,
                               ACMatchClassifications());

  spans = classify_test10.RunTest(u"comcast web login");
  //                           comcast.net web mail login
  // ACMatch spans should be: "-------MMMMM---MMMMMM-----";
  EXPECT_EQ("0,0,7,2,12,0,15,2,21,0",
            AutocompleteMatch::ClassificationsToString(spans));

  // Same with |classify_test10| except using characters in
  // base::kWhitespaceASCIIAs16 instead of white space.
  ClassifyTest classify_test11(u"comcast.net\x0aweb\x0dmail login",
                               /*text_is_query=*/true,
                               ACMatchClassifications());

  spans = classify_test11.RunTest(u"comcast web login");
  //                           comcast.net web mail login
  // ACMatch spans should be: "-------MMMMM---MMMMMM-----";
  EXPECT_EQ("0,0,7,2,12,0,15,2,21,0",
            AutocompleteMatch::ClassificationsToString(spans));
}

TEST_F(AutocompleteProviderTest, ResizeMatches) {
  TestProvider* provider = nullptr;
  ResetControllerWithTestProviders(false, &provider, nullptr);

  // Populate 'matches_` with test data.
  ACMatches matches = {
      AutocompleteMatch(nullptr, 100, false,
                        AutocompleteMatchType::BOOKMARK_TITLE),
      AutocompleteMatch(nullptr, 110, false,
                        AutocompleteMatchType::BOOKMARK_TITLE),
      AutocompleteMatch(nullptr, 120, false,
                        AutocompleteMatchType::BOOKMARK_TITLE),
      AutocompleteMatch(nullptr, 130, false,
                        AutocompleteMatchType::BOOKMARK_TITLE),
      AutocompleteMatch(nullptr, 140, false,
                        AutocompleteMatchType::BOOKMARK_TITLE),
      AutocompleteMatch(nullptr, 150, false,
                        AutocompleteMatchType::BOOKMARK_TITLE),
  };
  provider->set_matches(matches);
  EXPECT_EQ(provider->get_matches().size(), matches.size());

  // When ML Scoring is enabled, calling resize matches should not actually
  // resize the match list. Instead, it should mark any matches over the
  // `max_matches` with a relevance score of zero and `culled_by_provider`.
  const size_t kMaxMatches = 3;
  provider->ResizeMatches(kMaxMatches, true);
  EXPECT_EQ(provider->get_matches().size(), matches.size());

  // Check to see if `relevance` and `culled_by_provider` are set correctly.
  // The first `max_matches` matches should keep their relevance score and have
  // `culled_by_provider` set to false.
  ACMatches provider_matches = provider->get_matches();
  base::ranges::for_each(provider_matches.begin(),
                         std::next(provider_matches.begin(), kMaxMatches),
                         [&](auto match) {
                           EXPECT_NE(match.relevance, 0);
                           EXPECT_FALSE(match.culled_by_provider);
                         });
  // Any match beyond that should have their relevance score zeroed and
  // `culled_by_provider` set.
  base::ranges::for_each(std::next(provider_matches.begin(), kMaxMatches),
                         provider_matches.end(), [&](auto match) {
                           EXPECT_EQ(match.relevance, 0);
                           EXPECT_TRUE(match.culled_by_provider);
                         });

  // Now disable the flag. With ML Scoring disabled, `matches_` should actually
  // be resized and `relevance` and `culled_by_provider` should be untouched.
  provider->set_matches(matches);
  EXPECT_EQ(provider->get_matches().size(), matches.size());

  provider->ResizeMatches(kMaxMatches, false);
  EXPECT_EQ(provider->get_matches().size(), kMaxMatches);
  base::ranges::for_each(provider->get_matches(), [&](auto match) {
    EXPECT_NE(match.relevance, 0);
    EXPECT_FALSE(match.culled_by_provider);
  });
}

class AutocompleteProviderPrefetchTest : public AutocompleteProviderTest {
 public:
  AutocompleteProviderPrefetchTest() {
    RegisterTemplateURL(kTestTemplateURLKeyword,
                        "http://foo/{searchTerms}/{google:assistedQueryStats}");
    // Create an empty controller.
    ResetControllerWithType(0);
    provider_listener_ =
        std::make_unique<TestAutocompleteProviderListener>(controller_.get());
  }
  ~AutocompleteProviderPrefetchTest() override = default;
  AutocompleteProviderPrefetchTest(const AutocompleteProviderPrefetchTest&) =
      delete;
  AutocompleteProviderPrefetchTest& operator=(
      const AutocompleteProviderPrefetchTest&) = delete;

 protected:
  std::unique_ptr<TestAutocompleteProviderListener> provider_listener_;
};

TEST_F(AutocompleteProviderPrefetchTest, SupportedProvider_NonPrefetch) {
  // Add a test provider that supports prefetch requests.
  TestProvider* provider = new TestProvider(kResultsPerProvider, u"http://a",
                                            kTestTemplateURLKeyword, client_);
  provider->set_supports_prefetch(true);
  controller_->providers_.push_back(provider);

  base::RunLoop listener_run_loop;
  provider_listener_->set_closure(
      listener_run_loop.QuitClosure().Then(base::BindRepeating(
          &AutocompleteProviderTest::CopyResults, base::Unretained(this))));
  provider->AddListener(provider_listener_.get());

  AutocompleteInput input(u"foo", metrics::OmniboxEventProto::OTHER,
                          TestingSchemeClassifier());
  controller_->Start(input);

  ASSERT_FALSE(provider->done());
  ASSERT_FALSE(controller_->done());

  // Wait for the provider to finish asynchronously.
  listener_run_loop.Run();
  ASSERT_TRUE(provider->done());
  ASSERT_TRUE(controller_->done());

  // The results are expected to be non-empty as the provider did notify the
  // controller of the non-prefetch request results.
  EXPECT_EQ(kResultsPerProvider, result_.size());
}

TEST_F(AutocompleteProviderPrefetchTest, SupportedProvider_Prefetch) {
  // Add a test provider that supports prefetch requests.
  TestProvider* provider = new TestProvider(kResultsPerProvider, u"http://a",
                                            kTestTemplateURLKeyword, client_);
  provider->set_supports_prefetch(true);
  controller_->providers_.push_back(provider);

  base::RunLoop provider_run_loop;
  provider->set_closure(provider_run_loop.QuitClosure());

  base::RunLoop listener_run_loop;
  provider_listener_->set_closure(
      listener_run_loop.QuitClosure().Then(base::BindRepeating(
          &AutocompleteProviderTest::CopyResults, base::Unretained(this))));
  provider->AddListener(provider_listener_.get());

  AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                          TestingSchemeClassifier());
  controller_->StartPrefetch(input);
  // Wait for StartPrefetch() to be called on the provider.
  provider_run_loop.Run();

  // StartPrefetch() doesn't affect the state of the provider or the controller.
  ASSERT_FALSE(provider->prefetch_done());
  ASSERT_TRUE(provider->done());
  ASSERT_TRUE(controller_->done());

  // Wait for the provider to finish asynchronously.
  listener_run_loop.Run();
  ASSERT_TRUE(provider->prefetch_done());
  ASSERT_TRUE(controller_->done());

  // The results are expected to be empty as the provider did not notify the
  // controller of the prefetch request results.
  EXPECT_TRUE(result_.empty());
}

TEST_F(AutocompleteProviderPrefetchTest, SupportedProvider_OngoingNonPrefetch) {
  // Add a test provider that supports prefetch requests.
  TestProvider* provider = new TestProvider(kResultsPerProvider, u"http://a",
                                            kTestTemplateURLKeyword, client_);
  provider->set_supports_prefetch(true);
  controller_->providers_.push_back(provider);

  base::RunLoop provider_run_loop;
  provider->set_closure(provider_run_loop.QuitClosure());

  base::RunLoop listener_run_loop;
  provider_listener_->set_closure(
      listener_run_loop.QuitClosure().Then(base::BindRepeating(
          &AutocompleteProviderTest::CopyResults, base::Unretained(this))));
  provider->AddListener(provider_listener_.get());

  AutocompleteInput input(u"bar", metrics::OmniboxEventProto::OTHER,
                          TestingSchemeClassifier());
  controller_->Start(input);

  ASSERT_TRUE(provider->prefetch_done());
  ASSERT_FALSE(provider->done());
  ASSERT_FALSE(controller_->done());

  // Try to start a prefetch request while a non-prefetch request is still in
  // progress. We expect this not to call StartPrefetch() on the provider.
  // We test this by requesting synchronous matches from TestPrefetchProvider
  // which notifies the provider listener synchronously and calls the quit
  // closure of `run_loop` before `run_loop` is run. This prevents the provider
  // from being able to notify the controller of finishing the non-prefetch
  // request resulting in the controller to remain in an invalid state.
  input.set_omit_asynchronous_matches(true);
  controller_->StartPrefetch(input);

  ASSERT_FALSE(provider_run_loop.running());
  ASSERT_TRUE(provider->prefetch_done());

  // Wait for the provider to finish the non-prefetch request asynchronously.
  listener_run_loop.Run();
  ASSERT_TRUE(provider->done());
  ASSERT_TRUE(controller_->done());

  // The results are expected to be non-empty as the provider did notify the
  // controller of the non-prefetch request results.
  EXPECT_EQ(kResultsPerProvider, result_.size());
}

TEST_F(AutocompleteProviderPrefetchTest, UnsupportedProvider_Prefetch) {
  // Add a test provider that does not support prefetch requests.
  TestProvider* provider = new TestProvider(kResultsPerProvider, u"http://a",
                                            kTestTemplateURLKeyword, client_);
  controller_->providers_.push_back(provider);

  base::RunLoop provider_run_loop;
  provider->set_closure(provider_run_loop.QuitClosure());

  AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                          TestingSchemeClassifier());
  controller_->StartPrefetch(input);

  // We expect this not to call StartPrefetch() on the provider.
  ASSERT_FALSE(provider_run_loop.running());
  ASSERT_TRUE(provider->prefetch_done());
  ASSERT_TRUE(provider->done());
  ASSERT_TRUE(controller_->done());
}
