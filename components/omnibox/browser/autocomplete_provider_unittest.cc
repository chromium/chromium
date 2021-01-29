// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_provider.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/open_from_clipboard/fake_clipboard_recent_content.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/image/image_util.h"
#include "url/url_constants.h"

namespace {

const size_t kResultsPerProvider = 3;
const char kTestTemplateURLKeyword[] = "t";

class TestingSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  TestingSchemeClassifier() {}
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

// AutocompleteProviderClient implementation that calls the specified closure
// when the result is ready.
class AutocompleteProviderClientWithClosure
    : public MockAutocompleteProviderClient {
 public:
  AutocompleteProviderClientWithClosure() = default;
  AutocompleteProviderClientWithClosure(
      const AutocompleteProviderClientWithClosure&) = delete;
  AutocompleteProviderClientWithClosure& operator=(
      const AutocompleteProviderClientWithClosure&) = delete;

  void set_closure(const base::RepeatingClosure& closure) {
    closure_ = closure;
  }

 private:
  void OnAutocompleteControllerResultReady(
      AutocompleteController* controller) override {
    if (closure_)
      closure_.Run();
    if (base::RunLoop::IsRunningOnCurrentThread())
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  base::RepeatingClosure closure_;
};

}  // namespace

// Autocomplete provider that provides known results. Note that this is
// refcounted so that it can also be a task on the message loop.
class TestProvider : public AutocompleteProvider {
 public:
  TestProvider(int relevance,
               const base::string16& prefix,
               const base::string16& match_keyword,
               AutocompleteProviderClient* client)
      : AutocompleteProvider(AutocompleteProvider::TYPE_SEARCH),
        listener_(nullptr),
        relevance_(relevance),
        prefix_(prefix),
        match_keyword_(match_keyword),
        client_(client) {}
  TestProvider(const TestProvider&) = delete;
  TestProvider& operator=(const TestProvider&) = delete;

  void Start(const AutocompleteInput& input, bool minimal_changes) override;

  void set_listener(AutocompleteProviderListener* listener) {
    listener_ = listener;
  }

 private:
  ~TestProvider() override {}

  void Run();

  void AddResults(int start_at, int num);
  void AddResultsWithSearchTermsArgs(
      int start_at,
      int num,
      AutocompleteMatch::Type type,
      const TemplateURLRef::SearchTermsArgs& search_terms_args);

  AutocompleteProviderListener* listener_;
  int relevance_;
  const base::string16 prefix_;
  const base::string16 match_keyword_;
  AutocompleteProviderClient* client_;
};

void TestProvider::Start(const AutocompleteInput& input, bool minimal_changes) {
  if (minimal_changes)
    return;

  matches_.clear();

  if (input.focus_type() != OmniboxFocusType::DEFAULT)
    return;

  // Generate 4 results synchronously, the rest later.
  AddResults(0, 1);
  AddResultsWithSearchTermsArgs(
      1, 1, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
      TemplateURLRef::SearchTermsArgs(base::ASCIIToUTF16("echo")));
  AddResultsWithSearchTermsArgs(
      2, 1, AutocompleteMatchType::NAVSUGGEST,
      TemplateURLRef::SearchTermsArgs(base::ASCIIToUTF16("nav")));
  AddResultsWithSearchTermsArgs(
      3, 1, AutocompleteMatchType::SEARCH_SUGGEST,
      TemplateURLRef::SearchTermsArgs(base::ASCIIToUTF16("query")));

  if (input.want_asynchronous_matches()) {
    done_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&TestProvider::Run, this));
  }
}

void TestProvider::Run() {
  AddResults(1, kResultsPerProvider);
  done_ = true;
  DCHECK(listener_);
  listener_->OnProviderUpdate(true);
}

void TestProvider::AddResults(int start_at, int num) {
  AddResultsWithSearchTermsArgs(
      start_at, num, AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      TemplateURLRef::SearchTermsArgs(base::string16()));
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
    match.search_terms_args.reset(
        new TemplateURLRef::SearchTermsArgs(search_terms_args));
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
  ClassifyTest(const base::string16& text,
               const bool text_is_query,
               ACMatchClassifications matches);
  ~ClassifyTest();

  ACMatchClassifications RunTest(const base::string16& find_text);

 private:
  const base::string16 text_;
  const bool text_is_query_;
  const ACMatchClassifications matches_;
};

ClassifyTest::ClassifyTest(const base::string16& text,
                           const bool text_is_query,
                           ACMatchClassifications matches)
    : text_(text), text_is_query_(text_is_query), matches_(matches) {}

ClassifyTest::~ClassifyTest() {}

ACMatchClassifications ClassifyTest::RunTest(const base::string16& find_text) {
  return AutocompleteProvider::ClassifyAllMatchesInString(
      find_text, text_, text_is_query_, matches_);
}

class AutocompleteProviderTest : public testing::Test {
 public:
  AutocompleteProviderTest();
  ~AutocompleteProviderTest() override;
  AutocompleteProviderTest(const AutocompleteProviderTest&) = delete;
  AutocompleteProviderTest& operator=(const AutocompleteProviderTest&) = delete;

 protected:
  struct KeywordTestData {
    const base::string16 fill_into_edit;
    const base::string16 keyword;
    const base::string16 expected_associated_keyword;
  };

  struct HeaderTestData {
    SearchSuggestionParser::HeadersMap headers_map;
    std::vector<base::Optional<int>> suggestion_group_ids;
  };

  struct AssistedQueryStatsTestData {
    const AutocompleteMatch::Type match_type;
    const std::string expected_aqs;
    base::flat_set<int> subtypes;
  };

  // Registers a test TemplateURL under the given keyword.
  void RegisterTemplateURL(const base::string16& keyword,
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
  void RunKeywordTest(const base::string16& input,
                      const KeywordTestData* match_data,
                      size_t size);

  void UpdateResultsWithHeaderTestData(const HeaderTestData& headers_data);

  void RunAssistedQueryStatsTest(
      const AssistedQueryStatsTestData* aqs_test_data,
      size_t size);

  void RunQuery(const std::string& query, bool allow_exact_keyword_match);

  void ResetControllerWithKeywordAndSearchProviders();
  void ResetControllerWithKeywordProvider();
  void RunExactKeymatchTest(bool allow_exact_keyword_match);

  void CopyResults();

  // Returns match.destination_url as it would be set by
  // AutocompleteController::UpdateMatchDestinationURL().
  GURL GetDestinationURL(AutocompleteMatch& match,
                         base::TimeDelta query_formulation_time) const;

  // Returns the image from the clipboard as it would be from
  // AutocompleteController::GetImageFromClipboard().
  base::Optional<gfx::Image> GetImageFromClipboard() const;

  void set_search_provider_field_trial_triggered_in_session(bool val) {
    controller_->search_provider_->set_field_trial_triggered_in_session(val);
  }
  bool search_provider_field_trial_triggered_in_session() {
    return controller_->search_provider_->field_trial_triggered_in_session();
  }
  void set_current_page_classification(
      metrics::OmniboxEventProto::PageClassification classification) {
    controller_->input_.current_page_classification_ = classification;
  }
  void add_zero_suggest_provider_experiment_stat(
      const base::Value& experiment_stat) {
    auto& experiment_stats =
        const_cast<SearchSuggestionParser::ExperimentStats&>(
            controller_->zero_suggest_provider_->experiment_stats());
    experiment_stats.push_back(experiment_stat.Clone());
  }
  void add_zero_suggest_provider_headers_map(
      const SearchSuggestionParser::HeadersMap& headers_map) {
    auto& provider_headers_map =
        const_cast<SearchSuggestionParser::HeadersMap&>(
            controller_->zero_suggest_provider_->headers_map());
    provider_headers_map = headers_map;
  }

  TestingPrefServiceSimple* GetPrefs() { return &pref_service_; }

  AutocompleteResult result_;

 private:
  // Resets the controller with the given |type|. |type| is a bitmap containing
  // AutocompleteProvider::Type values that will (potentially, depending on
  // platform, flags, etc.) be instantiated.
  void ResetControllerWithType(int type);

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<AutocompleteController> controller_;
  // Owned by |controller_|.
  AutocompleteProviderClientWithClosure* client_;
  // Used to ensure that |client_| ownership has been passed to |controller_|
  // exactly once.
  bool client_owned_;
};

AutocompleteProviderTest::AutocompleteProviderTest()
    : client_(new AutocompleteProviderClientWithClosure()),
      client_owned_(false) {
  client_->set_template_url_service(
      std::make_unique<TemplateURLService>(nullptr, 0));
}

AutocompleteProviderTest::~AutocompleteProviderTest() {
  EXPECT_TRUE(client_owned_);
}

void AutocompleteProviderTest::RegisterTemplateURL(
    const base::string16& keyword,
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
  RegisterTemplateURL(base::ASCIIToUTF16(kTestTemplateURLKeyword),
                      "http://aqs/{searchTerms}/{google:assistedQueryStats}");

  AutocompleteController::Providers providers;

  // Construct two new providers, with either the same or different prefixes.
  TestProvider* provider1 =
      new TestProvider(kResultsPerProvider, base::ASCIIToUTF16("http://a"),
                       base::ASCIIToUTF16(kTestTemplateURLKeyword), client_);
  providers.push_back(provider1);

  TestProvider* provider2 = new TestProvider(
      kResultsPerProvider * 2,
      base::ASCIIToUTF16(same_destinations ? "http://a" : "http://b"),
      base::string16(), client_);
  providers.push_back(provider2);

  // Reset the controller to contain our new providers.
  ResetControllerWithType(0);

  // We're going to swap the providers vector, but the old vector should be
  // empty so no elements need to be freed at this point.
  EXPECT_TRUE(controller_->providers_.empty());
  controller_->providers_.swap(providers);
  provider1->set_listener(controller_.get());
  provider2->set_listener(controller_.get());

  client_->set_closure(base::BindRepeating(
      &AutocompleteProviderTest::CopyResults, base::Unretained(this)));

  if (provider1_ptr)
    *provider1_ptr = provider1;
  if (provider2_ptr)
    *provider2_ptr = provider2;
}

void AutocompleteProviderTest::ResetControllerWithKeywordAndSearchProviders() {
  // Reset the default TemplateURL.
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16("default"));
  data.SetKeyword(base::ASCIIToUTF16("default"));
  data.SetURL("http://defaultturl/{searchTerms}");
  TemplateURLService* turl_model = client_->GetTemplateURLService();
  TemplateURL* default_turl =
      turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_turl);
  TemplateURLID default_provider_id = default_turl->id();
  ASSERT_NE(0, default_provider_id);

  // Create another TemplateURL for KeywordProvider.
  TemplateURLData data2;
  data2.SetShortName(base::ASCIIToUTF16("k"));
  data2.SetKeyword(base::ASCIIToUTF16("k"));
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
  data.SetShortName(base::ASCIIToUTF16("foo.com"));
  data.SetKeyword(base::ASCIIToUTF16("foo.com"));
  data.SetURL("http://foo.com/{searchTerms}");
  TemplateURL* keyword_turl =
      turl_model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_NE(0, keyword_turl->id());

  // Make a TemplateURL for KeywordProvider that a shorter version of the
  // first.
  data.SetShortName(base::ASCIIToUTF16("f"));
  data.SetKeyword(base::ASCIIToUTF16("f"));
  data.SetURL("http://f.com/{searchTerms}");
  keyword_turl = turl_model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_NE(0, keyword_turl->id());

  // Create another TemplateURL for KeywordProvider.
  data.SetShortName(base::ASCIIToUTF16("bar.com"));
  data.SetKeyword(base::ASCIIToUTF16("bar.com"));
  data.SetURL("http://bar.com/{searchTerms}");
  keyword_turl = turl_model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_NE(0, keyword_turl->id());

  ResetControllerWithType(AutocompleteProvider::TYPE_KEYWORD);
}

void AutocompleteProviderTest::ResetControllerWithType(int type) {
  EXPECT_FALSE(client_owned_);
  controller_.reset(
      new AutocompleteController(base::WrapUnique(client_), type));
  client_owned_ = true;
}

void AutocompleteProviderTest::RunTest() {
  RunQuery("a", true);
}

void AutocompleteProviderTest::RunKeywordTest(const base::string16& input,
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
  result.AppendMatches(controller_->input_, matches);
  controller_->UpdateAssociatedKeywords(&result);
  for (size_t j = 0; j < result.size(); ++j) {
    EXPECT_EQ(match_data[j].expected_associated_keyword,
              result.match_at(j)->associated_keyword
                  ? result.match_at(j)->associated_keyword->keyword
                  : base::string16());
  }
}

void AutocompleteProviderTest::UpdateResultsWithHeaderTestData(
    const HeaderTestData& headers_data) {
  // Prepare.
  size_t relevance = 1000;
  ACMatches matches;
  for (auto suggestion_group_id : headers_data.suggestion_group_ids) {
    AutocompleteMatch match(nullptr, relevance--, false,
                            AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED);
    match.suggestion_group_id = suggestion_group_id;
    matches.push_back(match);
  }

  add_zero_suggest_provider_headers_map(headers_data.headers_map);

  result_.Reset();
  result_.AppendMatches(AutocompleteInput(), matches);

  // Update the result with the header information.
  controller_->UpdateHeaderInfoFromZeroSuggestProvider(&result_);
  // Group matches with headers and move them to the bottom of the result set.
  result_.GroupAndDemoteMatchesWithHeaders();
}

void AutocompleteProviderTest::RunAssistedQueryStatsTest(
    const AssistedQueryStatsTestData* aqs_test_data,
    size_t size) {
  // Prepare input.
  const size_t kMaxRelevance = 1000;
  ACMatches matches;
  for (size_t i = 0; i < size; ++i) {
    AutocompleteMatch match(nullptr, kMaxRelevance - i, false,
                            aqs_test_data[i].match_type);
    match.allowed_to_be_default_match = true;
    match.keyword = base::ASCIIToUTF16(kTestTemplateURLKeyword);
    match.search_terms_args.reset(
        new TemplateURLRef::SearchTermsArgs(base::string16()));
    match.subtypes = aqs_test_data[i].subtypes;
    matches.push_back(match);
  }
  result_.Reset();
  result_.AppendMatches(AutocompleteInput(), matches);

  // Update AQS.
  controller_->UpdateAssistedQueryStats(&result_);

  // Verify data.
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(aqs_test_data[i].expected_aqs,
              result_.match_at(i)->search_terms_args->assisted_query_stats);
  }
}

void AutocompleteProviderTest::RunQuery(const std::string& query,
                                        bool allow_exact_keyword_match) {
  result_.Reset();
  AutocompleteInput input(base::ASCIIToUTF16(query),
                          metrics::OmniboxEventProto::OTHER,
                          TestingSchemeClassifier());
  input.set_prevent_inline_autocomplete(true);
  input.set_allow_exact_keyword_match(allow_exact_keyword_match);
  controller_->Start(input);

  if (!controller_->done())
    // The message loop will terminate when all autocomplete input has been
    // collected.
    base::RunLoop().Run();
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
  result_.CopyFrom(controller_->result());
}

GURL AutocompleteProviderTest::GetDestinationURL(
    AutocompleteMatch& match,
    base::TimeDelta query_formulation_time) const {
  controller_->UpdateMatchDestinationURLWithQueryFormulationTime(
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

// Tests assisted query stats.
TEST_F(AutocompleteProviderTest, AssistedQueryStats) {
  ResetControllerWithTestProviders(false, nullptr, nullptr);
  RunTest();

  ASSERT_EQ(
      std::min(AutocompleteResult::GetMaxMatches(), kResultsPerProvider * 2),
      result_.size());

  // Now, check the results from the second provider, as they should not have
  // assisted query stats set.
  for (size_t i = 0; i < kResultsPerProvider; ++i) {
    EXPECT_TRUE(
        result_.match_at(i)->search_terms_args->assisted_query_stats.empty());
  }
  // The first provider has a test keyword, so AQS should be non-empty.
  for (size_t i = kResultsPerProvider; i < result_.size(); ++i) {
    EXPECT_FALSE(
        result_.match_at(i)->search_terms_args->assisted_query_stats.empty());
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
  ASSERT_EQ(2U, result_.size());
  EXPECT_EQ("http://keyword/test",
            result_.match_at(0)->destination_url.possibly_invalid_spec());
  EXPECT_EQ("http://defaultturl/k%20test?a=b",
            result_.match_at(1)->destination_url.possibly_invalid_spec());
}

// Test that redundant associated keywords are removed.
TEST_F(AutocompleteProviderTest, RedundantKeywordsIgnoredInResult) {
  ResetControllerWithKeywordProvider();

  {
    KeywordTestData duplicate_url[] = {
        {base::ASCIIToUTF16("fo"), base::string16(), base::string16()},
        {base::ASCIIToUTF16("foo.com"), base::string16(),
         base::ASCIIToUTF16("foo.com")},
        {base::ASCIIToUTF16("foo.com"), base::string16(), base::string16()}};

    SCOPED_TRACE("Duplicate url");
    RunKeywordTest(base::ASCIIToUTF16("fo"), duplicate_url,
                   base::size(duplicate_url));
  }

  {
    KeywordTestData keyword_match[] = {
        {base::ASCIIToUTF16("foo.com"), base::ASCIIToUTF16("foo.com"),
         base::string16()},
        {base::ASCIIToUTF16("foo.com"), base::string16(), base::string16()}};

    SCOPED_TRACE("Duplicate url with keyword match");
    RunKeywordTest(base::ASCIIToUTF16("fo"), keyword_match,
                   base::size(keyword_match));
  }

  {
    KeywordTestData multiple_keyword[] = {
        {base::ASCIIToUTF16("fo"), base::string16(), base::string16()},
        {base::ASCIIToUTF16("foo.com"), base::string16(),
         base::ASCIIToUTF16("foo.com")},
        {base::ASCIIToUTF16("foo.com"), base::string16(), base::string16()},
        {base::ASCIIToUTF16("bar.com"), base::string16(),
         base::ASCIIToUTF16("bar.com")},
    };

    SCOPED_TRACE("Duplicate url with multiple keywords");
    RunKeywordTest(base::ASCIIToUTF16("fo"), multiple_keyword,
                   base::size(multiple_keyword));
  }
}

// Test that exact match keywords trump keywords associated with
// the match.
TEST_F(AutocompleteProviderTest, ExactMatchKeywords) {
  ResetControllerWithKeywordProvider();

  {
    KeywordTestData keyword_match[] = {{base::ASCIIToUTF16("foo.com"),
                                        base::string16(),
                                        base::ASCIIToUTF16("foo.com")}};

    SCOPED_TRACE("keyword match as usual");
    RunKeywordTest(base::ASCIIToUTF16("fo"), keyword_match,
                   base::size(keyword_match));
  }

  // The same result set with an input of "f" (versus "fo") should get
  // a different associated keyword because "f" is an exact match for
  // a keyword and that should trump the keyword normally associated with
  // this match.
  {
    KeywordTestData keyword_match[] = {{base::ASCIIToUTF16("foo.com"),
                                        base::string16(),
                                        base::ASCIIToUTF16("f")}};

    SCOPED_TRACE("keyword exact match");
    RunKeywordTest(base::ASCIIToUTF16("f"), keyword_match,
                   base::size(keyword_match));
  }
}

// Tests that the AutocompleteResult is updated with the header information and
// matches with headers are grouped and demoted correctly.
TEST_F(AutocompleteProviderTest, Headers) {
  ResetControllerWithKeywordAndSearchProviders();

  const int kRecommendedForYouGroupId = 1;
  const char kRecommendedForYouHeader[] = "Recommended for you";
  const int kRecentSearchesGroupId = 2;
  const char kRecentSearchesHeader[] = "Recent Searches";

  // This exists to verify that we ignore group IDs without associated header
  // text when sorting results.
  const int kGroupIdWithoutHeaderText = 99;

  SearchSuggestionParser::HeadersMap headers_map = {
      {kRecommendedForYouGroupId, base::ASCIIToUTF16(kRecommendedForYouHeader)},
      {kRecentSearchesGroupId, base::ASCIIToUTF16(kRecentSearchesHeader)}};

  {
    HeaderTestData test_data = {headers_map,
                                {{base::nullopt},
                                 {base::nullopt},
                                 {base::nullopt},
                                 {kRecentSearchesGroupId},
                                 {kRecommendedForYouGroupId}}};
    UpdateResultsWithHeaderTestData(test_data);
    EXPECT_FALSE(result_.match_at(0)->suggestion_group_id.has_value());
    EXPECT_FALSE(result_.match_at(1)->suggestion_group_id.has_value());
    EXPECT_FALSE(result_.match_at(2)->suggestion_group_id.has_value());
    EXPECT_EQ(kRecentSearchesGroupId,
              result_.match_at(3)->suggestion_group_id.value());
    EXPECT_EQ(kRecommendedForYouGroupId,
              result_.match_at(4)->suggestion_group_id.value());

    // Verify that AutocompleteResult is updated with the header information.
    EXPECT_EQ(base::ASCIIToUTF16(kRecommendedForYouHeader),
              result_.GetHeaderForGroupId(kRecommendedForYouGroupId));
    EXPECT_EQ(base::ASCIIToUTF16(kRecentSearchesHeader),
              result_.GetHeaderForGroupId(kRecentSearchesGroupId));
    EXPECT_EQ(base::string16(), result_.GetHeaderForGroupId(-1));
  }
  {
    HeaderTestData test_data = {headers_map,
                                {
                                    {base::nullopt},
                                    {kRecentSearchesGroupId},
                                    {base::nullopt},
                                    {kRecommendedForYouGroupId},
                                    {kRecentSearchesGroupId},
                                }};
    UpdateResultsWithHeaderTestData(test_data);

    // Verifies that matches with group IDs are grouped and sink to the bottom.
    EXPECT_FALSE(result_.match_at(0)->suggestion_group_id.has_value());
    EXPECT_FALSE(result_.match_at(1)->suggestion_group_id.has_value());
    EXPECT_EQ(kRecentSearchesGroupId,
              result_.match_at(2)->suggestion_group_id.value());
    EXPECT_EQ(kRecentSearchesGroupId,
              result_.match_at(3)->suggestion_group_id.value());
    EXPECT_EQ(kRecommendedForYouGroupId,
              result_.match_at(4)->suggestion_group_id.value());
  }
  {
    HeaderTestData test_data = {headers_map,
                                {{kGroupIdWithoutHeaderText},
                                 {kRecentSearchesGroupId},
                                 {kRecommendedForYouGroupId},
                                 {kGroupIdWithoutHeaderText},
                                 {kGroupIdWithoutHeaderText}}};
    UpdateResultsWithHeaderTestData(test_data);

    // Verifies that group IDs without associated header text are stripped out,
    // and those matches float to the top.
    EXPECT_FALSE(result_.match_at(0)->suggestion_group_id.has_value());
    EXPECT_FALSE(result_.match_at(1)->suggestion_group_id.has_value());
    EXPECT_FALSE(result_.match_at(2)->suggestion_group_id.has_value());
    EXPECT_EQ(kRecentSearchesGroupId,
              result_.match_at(3)->suggestion_group_id.value());
    EXPECT_EQ(kRecommendedForYouGroupId,
              result_.match_at(4)->suggestion_group_id.value());
  }
}

TEST_F(AutocompleteProviderTest, UpdateAssistedQueryStats) {
  ResetControllerWithTestProviders(false, nullptr, nullptr);

  {
    AssistedQueryStatsTestData test_data[] = {
        //  MSVC doesn't support zero-length arrays, so supply some dummy data.
        {AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, ""}};
    SCOPED_TRACE("No matches");
    // Note: We pass 0 here to ignore the dummy data above.
    RunAssistedQueryStatsTest(test_data, 0);
  }

  {
    AssistedQueryStatsTestData test_data[] = {
        {AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, "chrome..69i57"}};
    SCOPED_TRACE("One match");
    RunAssistedQueryStatsTest(test_data, base::size(test_data));
  }

  {
    AssistedQueryStatsTestData test_data[] = {
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         "chrome.0.46i131",
         {131}}};
    SCOPED_TRACE("One match with provider populated subtypes");
    RunAssistedQueryStatsTest(test_data, base::size(test_data));
  }

  {
    // This test confirms that repetitive subtype information is being
    // properly handled and reported as the same suggestion type.
    AssistedQueryStatsTestData test_data[] = {
        {AutocompleteMatchType::SEARCH_SUGGEST,
         "chrome.0.0i13i22i99j46i27i31l2j46i27i31i42j46i27i31",
         {22, 99, 13, 99}},
        // The next two matches should be detected as the same type, despite
        // repeated subtype match.
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         "chrome.1.0i13i22i99j46i27i31l2j46i27i31i42j46i27i31",
         {27, 31}},
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         "chrome.2.0i13i22i99j46i27i31l2j46i27i31i42j46i27i31",
         {27, 31, 27}},
        // This match should not be bundled together with previous two, because
        // it comes with additional subtype information (42).
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         "chrome.3.0i13i22i99j46i27i31l2j46i27i31i42j46i27i31",
         {27, 31, 42}},
        // This match should not be bundled together with the group before,
        // because these items are not adjacent.
        {AutocompleteMatchType::SEARCH_SUGGEST_ENTITY,
         "chrome.4.0i13i22i99j46i27i31l2j46i27i31i42j46i27i31",
         {27, 31}},
    };
    SCOPED_TRACE("Complex set of matches with repetitive subtypes");
    RunAssistedQueryStatsTest(test_data, base::size(test_data));
  }

  {
    // This test confirms that we record the count of ZeroSuggest matches that
    // originate from the suggest server and are annotated with appropriate
    // metadata. The number of these matches is reported via AQS as a
    // NUM_ZERO_PREFIX_SHOWN.
    AssistedQueryStatsTestData test_data[] = {
        // Only the following subtypes should be counted:
        // - SUBTYPE_ZERO_PREFIX (362)
        // - SUBTYPE_ZERO_PREFIX_LOCAL_HISTORY (450)
        // - SUBTYPE_ZERO_PREFIX_LOCAL_FREQUENT_URL (451)
        {AutocompleteMatchType::SEARCH_SUGGEST,
         "chrome.0.0i362j0i450j5i361j5i451j5j0i19i362j0i7i13i362i450i451...5",
         {362}},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         "chrome.1.0i362j0i450j5i361j5i451j5j0i19i362j0i7i13i362i450i451...5",
         {450}},
        // Ignored because of not matching subtypes.
        {AutocompleteMatchType::NAVSUGGEST,
         "chrome.2.0i362j0i450j5i361j5i451j5j0i19i362j0i7i13i362i450i451...5",
         {361}},
        // Local most visited URL.
        {AutocompleteMatchType::NAVSUGGEST,
         "chrome.3.0i362j0i450j5i361j5i451j5j0i19i362j0i7i13i362i450i451...5",
         {451}},
        // Ignored because of no reported subtypes.
        {AutocompleteMatchType::NAVSUGGEST,
         "chrome.4.0i362j0i450j5i361j5i451j5j0i19i362j0i7i13i362i450i451...5",
         {}},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         "chrome.5.0i362j0i450j5i361j5i451j5j0i19i362j0i7i13i362i450i451...5",
         {19, 362}},
        // Counted once, despite reporting all  subtypes.
        {AutocompleteMatchType::SEARCH_SUGGEST,
         "chrome.6.0i362j0i450j5i361j5i451j5j0i19i362j0i7i13i362i450i451...5",
         {7, 13, 362, 450, 451}},
    };
    SCOPED_TRACE("Num Zero Suggest Shown reports");
    RunAssistedQueryStatsTest(test_data, base::size(test_data));
  }

  {
    AssistedQueryStatsTestData test_data[] = {
        {AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
         "chrome..69i57j69i58j5l2j0l3j69i59"},
        {AutocompleteMatchType::URL_WHAT_YOU_TYPED,
         "chrome..69i57j69i58j5l2j0l3j69i59"},
        {AutocompleteMatchType::NAVSUGGEST,
         "chrome.2.69i57j69i58j5l2j0l3j69i59"},
        {AutocompleteMatchType::NAVSUGGEST,
         "chrome.3.69i57j69i58j5l2j0l3j69i59"},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         "chrome.4.69i57j69i58j5l2j0l3j69i59"},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         "chrome.5.69i57j69i58j5l2j0l3j69i59"},
        {AutocompleteMatchType::SEARCH_SUGGEST,
         "chrome.6.69i57j69i58j5l2j0l3j69i59"},
        {AutocompleteMatchType::SEARCH_HISTORY,
         "chrome.7.69i57j69i58j5l2j0l3j69i59"},
    };
    SCOPED_TRACE("Multiple matches");
    RunAssistedQueryStatsTest(test_data, base::size(test_data));
  }
}

TEST_F(AutocompleteProviderTest, GetDestinationURL) {
  ResetControllerWithKeywordAndSearchProviders();

  // For the destination URL to have aqs parameters for query formulation time
  // and the field trial triggered bit, many conditions need to be satisfied.
  AutocompleteMatch match(nullptr, 1100, false,
                          AutocompleteMatchType::SEARCH_SUGGEST);
  GURL url(GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456)));
  EXPECT_TRUE(url.path().empty());

  // The protocol needs to be https.
  RegisterTemplateURL(base::ASCIIToUTF16(kTestTemplateURLKeyword),
                      "https://aqs/{searchTerms}/{google:assistedQueryStats}");
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // There needs to be a keyword provider.
  match.keyword = base::ASCIIToUTF16(kTestTemplateURLKeyword);
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // search_terms_args needs to be set.
  match.search_terms_args.reset(
      new TemplateURLRef::SearchTermsArgs(base::string16()));
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // assisted_query_stats needs to have been previously set.
  match.search_terms_args->assisted_query_stats =
      "chrome.0.69i57j69i58j5l2j0l3j69i59";
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ("//aqs=chrome.0.69i57j69i58j5l2j0l3j69i59.2456j0j0&", url.path());

  // Test field trial triggered bit set.
  set_search_provider_field_trial_triggered_in_session(true);
  EXPECT_TRUE(search_provider_field_trial_triggered_in_session());
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ("//aqs=chrome.0.69i57j69i58j5l2j0l3j69i59.2456j1j0&", url.path());

  // Test page classification set.
  set_current_page_classification(metrics::OmniboxEventProto::OTHER);
  set_search_provider_field_trial_triggered_in_session(false);
  EXPECT_FALSE(search_provider_field_trial_triggered_in_session());
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ("//aqs=chrome.0.69i57j69i58j5l2j0l3j69i59.2456j0j4&", url.path());

  // Test page classification and field trial triggered set.
  set_search_provider_field_trial_triggered_in_session(true);
  EXPECT_TRUE(search_provider_field_trial_triggered_in_session());
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ("//aqs=chrome.0.69i57j69i58j5l2j0l3j69i59.2456j1j4&", url.path());

  // Test experiment stats set.
  add_zero_suggest_provider_experiment_stat(
      base::test::ParseJson(R"json({"2":"0:67","4":10001})json"));
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ("//aqs=chrome.0.69i57j69i58j5l2j0l3j69i59.2456j1j4.10001i0,67&",
            url.path());
  add_zero_suggest_provider_experiment_stat(
      base::test::ParseJson(R"json({"2":"54:67","4":10001})json"));
  url = GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ(
      "//"
      "aqs=chrome.0.69i57j69i58j5l2j0l3j69i59.2456j1j4.10001i0,67j10001i54,67&",
      url.path());
}

TEST_F(AutocompleteProviderTest, ClassifyAllMatchesInString) {
  ResetControllerWithKeywordAndSearchProviders();

  using base::ASCIIToUTF16;
  ACMatchClassifications matches =
      AutocompleteMatch::ClassificationsFromString("0,0");
  ClassifyTest classify_test(ASCIIToUTF16("A man, a plan, a canal Panama"),
                             /*text_is_query=*/false, matches);

  ACMatchClassifications spans;

  spans = classify_test.RunTest(ASCIIToUTF16("man"));
  //                           A man, a plan, a canal Panama
  // ACMatch spans should be: '--MMM------------------------'
  EXPECT_EQ("0,0,2,2,5,0", AutocompleteMatch::ClassificationsToString(spans));

  spans = classify_test.RunTest(ASCIIToUTF16("man p"));
  //                           A man, a plan, a canal Panama
  // ACMatch spans should be: '--MMM----M-------------M-----'
  EXPECT_EQ("0,0,2,2,5,0,9,2,10,0,23,2,24,0",
            AutocompleteMatch::ClassificationsToString(spans));

  // Comparisons should be case insensitive.
  spans = classify_test.RunTest(ASCIIToUTF16("mAn pLAn panAMa"));
  //                           A man, a plan, a canal Panama
  // ACMatch spans should be: '--MMM----MMMM----------MMMMMM'
  EXPECT_EQ("0,0,2,2,5,0,9,2,13,0,23,2",
            AutocompleteMatch::ClassificationsToString(spans));

  // When the user input is a prefix of the suggest text, subsequent occurrences
  // of the user words should be matched.
  spans = classify_test.RunTest(ASCIIToUTF16("a man,"));
  //                           A man, a plan, a canal Panama
  // ACMatch spans should be: 'MMMMMM-----------------------'
  EXPECT_EQ("0,2,6,0", AutocompleteMatch::ClassificationsToString(spans));

  // Matches must begin at word-starts in the suggest text. E.g. 'a' in 'plan'
  // should not match.
  spans = classify_test.RunTest(ASCIIToUTF16("a man, p"));
  //                           A man, a plan, a canal Panama
  // ACMatch spans should be: 'M-MMM--M-M-----M-------M-----'
  EXPECT_EQ("0,2,1,0,2,2,5,0,7,2,8,0,9,2,10,0,15,2,16,0,23,2,24,0",
            AutocompleteMatch::ClassificationsToString(spans));

  ClassifyTest classify_test2(
      ASCIIToUTF16("Yahoo! Sports - Sports News, "
                   "Scores, Rumors, Fantasy Games, and more"),
      /*text_is_query=*/false, matches);

  spans = classify_test2.RunTest(ASCIIToUTF16("ne"));
  // Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more
  // -----------------------MM-------------------------------------------
  EXPECT_EQ("0,0,23,2,25,0", AutocompleteMatch::ClassificationsToString(spans));

  spans = classify_test2.RunTest(ASCIIToUTF16("neWs R"));
  // Yahoo! Sports - Sports News, Scores, Rumors, Fantasy Games, and more
  // -----------------------MMMM----------M------------------------------
  EXPECT_EQ("0,0,23,2,27,0,37,2,38,0",
            AutocompleteMatch::ClassificationsToString(spans));

  matches = AutocompleteMatch::ClassificationsFromString("0,1");
  ClassifyTest classify_test3(ASCIIToUTF16("livescore.goal.com"),
                              /*text_is_query=*/false, matches);

  // Matches should be merged with existing classifications.
  // Matches can begin after symbols in the suggest text.
  spans = classify_test3.RunTest(ASCIIToUTF16("go"));
  // livescore.goal.com
  // ----------MM------
  // ACMatch spans should match first two letters of the "goal".
  EXPECT_EQ("0,1,10,3,12,1", AutocompleteMatch::ClassificationsToString(spans));

  matches = AutocompleteMatch::ClassificationsFromString("0,0,13,1");
  ClassifyTest classify_test4(ASCIIToUTF16("Email login: mail.somecorp.com"),
                              /*text_is_query=*/false, matches);

  // Matches must begin at word-starts in the suggest text.
  spans = classify_test4.RunTest(ASCIIToUTF16("ail"));
  // Email login: mail.somecorp.com
  // 000000000000011111111111111111
  EXPECT_EQ("0,0,13,1", AutocompleteMatch::ClassificationsToString(spans));

  // The longest matches should take precedence (e.g. 'log' instead of 'lo').
  spans = classify_test4.RunTest(ASCIIToUTF16("lo log mail em"));
  // Email login: mail.somecorp.com
  // 220000222000033331111111111111
  EXPECT_EQ("0,2,2,0,6,2,9,0,13,3,17,1",
            AutocompleteMatch::ClassificationsToString(spans));

  // Some web sites do not have a description.  If the string being searched is
  // empty, the classifications must also be empty: http://crbug.com/148647
  // Extra parens in the next line hack around C++03's "most vexing parse".
  class ClassifyTest classify_test5((base::string16()), /*text_is_query=*/false,
                                    ACMatchClassifications());
  spans = classify_test5.RunTest(ASCIIToUTF16("man"));
  ASSERT_EQ(0U, spans.size());

  // Matches which end at beginning of classification merge properly.
  matches = AutocompleteMatch::ClassificationsFromString("0,4,9,0");
  ClassifyTest classify_test6(ASCIIToUTF16("html password example"),
                              /*text_is_query=*/false, matches);

  // Extra space in the next string avoids having the string be a prefix of the
  // text above, which would allow for two different valid classification sets,
  // one of which uses two spans (the first of which would mark all of "html
  // pass" as a match) and one which uses four (which marks the individual words
  // as matches but not the space between them).  This way only the latter is
  // valid.
  spans = classify_test6.RunTest(ASCIIToUTF16("html  pass"));
  EXPECT_EQ("0,6,4,4,5,6,9,0",
            AutocompleteMatch::ClassificationsToString(spans));

  // Multiple matches with both beginning and end at beginning of
  // classifications merge properly.
  matches = AutocompleteMatch::ClassificationsFromString("0,1,11,0");
  ClassifyTest classify_test7(ASCIIToUTF16("http://a.co is great"),
                              /*text_is_query=*/false, matches);

  spans = classify_test7.RunTest(ASCIIToUTF16("ht co"));
  EXPECT_EQ("0,3,2,1,9,3,11,0",
            AutocompleteMatch::ClassificationsToString(spans));

  // Search queries should be bold non-matches and unbold matches.
  matches = AutocompleteMatch::ClassificationsFromString("0,0");
  ClassifyTest classify_test8(ASCIIToUTF16("panama canal"),
                              /*text_is_query=*/true, matches);

  spans = classify_test8.RunTest(ASCIIToUTF16("pan"));
  //                           panama canal
  // ACMatch spans should be: "---MMMMMMMMM";
  EXPECT_EQ("0,0,3,2", AutocompleteMatch::ClassificationsToString(spans));
  spans = classify_test8.RunTest(ASCIIToUTF16("canal"));
  //                           panama canal
  // ACMatch spans should be: "MMMMMMM-----";
  EXPECT_EQ("0,2,7,0", AutocompleteMatch::ClassificationsToString(spans));

  // Search autocomplete suggestion.
  ClassifyTest classify_test9(ASCIIToUTF16("comcast webmail login"),
                              /*text_is_query=*/true, ACMatchClassifications());

  // Matches first and first part of middle word and the last word.
  spans = classify_test9.RunTest(ASCIIToUTF16("comcast web login"));
  //                           comcast webmail login
  // ACMatch spans should be: "-------M---MMMMM-----";
  EXPECT_EQ("0,0,7,2,8,0,11,2,16,0",
            AutocompleteMatch::ClassificationsToString(spans));

  // Matches partial word in the middle of suggestion.
  spans = classify_test9.RunTest(ASCIIToUTF16("web"));
  //                           comcast webmail login
  // ACMatch spans should be: "MMMMMMMM---MMMMMMMMMM";
  EXPECT_EQ("0,2,8,0,11,2", AutocompleteMatch::ClassificationsToString(spans));

  ClassifyTest classify_test10(ASCIIToUTF16("comcast.net web mail login"),
                              /*text_is_query=*/true, ACMatchClassifications());

  spans = classify_test10.RunTest(ASCIIToUTF16("comcast web login"));
  //                           comcast.net web mail login
  // ACMatch spans should be: "-------MMMMM---MMMMMM-----";
  EXPECT_EQ("0,0,7,2,12,0,15,2,21,0",
            AutocompleteMatch::ClassificationsToString(spans));

  // Same with |classify_test10| except using characters in
  // base::kWhitespaceASCIIAs16 instead of white space.
  ClassifyTest classify_test11(ASCIIToUTF16("comcast.net\x0aweb\x0dmail login"),
                              /*text_is_query=*/true, ACMatchClassifications());

  spans = classify_test11.RunTest(ASCIIToUTF16("comcast web login"));
  //                           comcast.net web mail login
  // ACMatch spans should be: "-------MMMMM---MMMMMM-----";
  EXPECT_EQ("0,0,7,2,12,0,15,2,21,0",
            AutocompleteMatch::ClassificationsToString(spans));
}
