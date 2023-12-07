// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings {
namespace {
TemplateURL* AddSearchEngine(TemplateURLService* template_url_service,
                             const std::string& name,
                             int prepopulated_id,
                             absl::optional<std::string> url) {
  TemplateURLData default_search_engine;
  default_search_engine.SetShortName(base::UTF8ToUTF16(name));
  default_search_engine.prepopulate_id = prepopulated_id;

  if (url.has_value()) {
    default_search_engine.SetURL(*url);
  } else {
    default_search_engine.SetURL("http://" + name +
                                 "foo.com/url?bar={searchTerms}");
  }
  default_search_engine.alternate_urls.push_back("http://" + name +
                                                 "/alt#quux={searchTerms}");
  return template_url_service->Add(
      std::make_unique<TemplateURL>(default_search_engine));
}
}  // namespace

class SearchEnginesHandlerTestBase : public testing::Test {
 public:
  SearchEnginesHandlerTestBase()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Profile 1");

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    TemplateURL* default_engine =
        AddSearchEngine(template_url_service, "foo.com", /*prepopulated_id=*/0,
                        /*url=*/absl::nullopt);
    AddSearchEngine(template_url_service, "bing",
                    TemplateURLPrepopulateData::bing.id,
                    TemplateURLPrepopulateData::bing.search_url);

    template_url_service->SetUserSelectedDefaultSearchProvider(default_engine);

    handler_ = std::make_unique<SearchEnginesHandler>(profile_);
    web_ui_.set_web_contents(web_contents_factory_.CreateWebContents(profile_));
    handler_->set_web_ui(&web_ui_);
    handler()->AllowJavascript();
    handler()->RegisterMessages();
    web_ui()->ClearTrackedCalls();
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Profile* profile() const { return profile_; }
  SearchEnginesHandler* handler() const { return handler_.get(); }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  content::TestWebContentsFactory web_contents_factory_;
  content::TestWebUI web_ui_;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<SearchEnginesHandler> handler_;
};

class SearchEnginesHandlerParametrizedTest
    : public SearchEnginesHandlerTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SearchEnginesHandlerParametrizedTest() {
    if (WithSearchEnginesChoiceEnabled()) {
      feature_list()->InitAndEnableFeature(switches::kSearchEngineChoice);
    }
  }

  void SetUp() override {
    SearchEnginesHandlerTestBase::SetUp();

    if (WithSearchEnginesChoiceEnabled()) {
      PrefService* pref_service = profile()->GetPrefs();
      // The search engine choice feature is only enabled for countries in the
      // EEA region.
      const int kBelgiumCountryId =
          country_codes::CountryCharsToCountryID('B', 'E');
      pref_service->SetInteger(country_codes::kCountryIDAtInstall,
                               kBelgiumCountryId);
    }
  }

  bool WithSearchEnginesChoiceEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         SearchEnginesHandlerParametrizedTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithSearchEngineChoiceEnabled"
                                             : "Default";
                         });

TEST_P(SearchEnginesHandlerParametrizedTest,
       ChangeInTemplateUrlDataTriggersCallback) {
  EXPECT_EQ(0U, web_ui()->call_data().size());
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURL* template_url =
      AddSearchEngine(template_url_service, "bar.com", /*prepopulated_id=*/0,
                      /*url=*/absl::nullopt);

  EXPECT_EQ(1U, web_ui()->call_data().size());
  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", call_data.function_name());
  EXPECT_EQ("search-engines-changed", call_data.arg1()->GetString());

  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  EXPECT_EQ(2U, web_ui()->call_data().size());
  const content::TestWebUI::CallData& second_call_data =
      *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", second_call_data.function_name());
  EXPECT_EQ("search-engines-changed", second_call_data.arg1()->GetString());
}

TEST_P(SearchEnginesHandlerParametrizedTest,
       SettingTheDefaultSearchEngineRecordsHistogram) {
  base::Value::List first_call_args;
  // Search engine model id.
  first_call_args.Append(1);
  first_call_args.Append(static_cast<int>(
      search_engines::ChoiceMadeLocation::kSearchEngineSettings));
  web_ui()->HandleReceivedMessage("setDefaultSearchEngine", first_call_args);

  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_BING,
      WithSearchEnginesChoiceEnabled() ? 1 : 0);

  base::Value::List second_call_args;
  // Search engine model id.
  second_call_args.Append(1);
  second_call_args.Append(
      static_cast<int>(search_engines::ChoiceMadeLocation::kSearchSettings));
  web_ui()->HandleReceivedMessage("setDefaultSearchEngine", second_call_args);

  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_BING,
      WithSearchEnginesChoiceEnabled() ? 1 : 0);
}

class SearchEnginesHandlerTestWithSearchEngineChoiceEnabled
    : public SearchEnginesHandlerTestBase {
 public:
  SearchEnginesHandlerTestWithSearchEngineChoiceEnabled() {
    feature_list()->InitAndEnableFeature(switches::kSearchEngineChoice);
  }

  void SetUp() override {
    SearchEnginesHandlerTestBase::SetUp();

    PrefService* pref_service = profile()->GetPrefs();
    // The search engine choice feature is only enabled for countries in the
    // EEA region.
    const int kBelgiumCountryId =
        country_codes::CountryCharsToCountryID('B', 'E');
    pref_service->SetInteger(country_codes::kCountryIDAtInstall,
                             kBelgiumCountryId);
  }
};

TEST_F(SearchEnginesHandlerTestWithSearchEngineChoiceEnabled,
       ModifyingSearchEngineSetsSearchEngineChoiceTimestamp) {
  PrefService* pref_service = profile()->GetPrefs();
  // The search engine choice feature is only enabled for countries in the EEA
  // region.
  const int kBelgiumCountryId =
      country_codes::CountryCharsToCountryID('B', 'E');
  pref_service->SetInteger(country_codes::kCountryIDAtInstall,
                           kBelgiumCountryId);

  EXPECT_FALSE(pref_service->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  base::Value::List args;
  // Search engine model id.
  args.Append(1);
  args.Append(static_cast<int>(
      search_engines::ChoiceMadeLocation::kSearchEngineSettings));
  web_ui()->HandleReceivedMessage("setDefaultSearchEngine", args);

  EXPECT_NEAR(pref_service->GetInt64(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
              base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds(),
              /*abs_error=*/2);
}

TEST_F(SearchEnginesHandlerTestWithSearchEngineChoiceEnabled,
       RecordingSearchEngineShouldBeDoneAfterSettingDefault) {
  PrefService* pref_service = profile()->GetPrefs();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  // The search engine choice feature is only enabled for countries in the EEA
  // region.
  const int kBelgiumCountryId =
      country_codes::CountryCharsToCountryID('B', 'E');
  pref_service->SetInteger(country_codes::kCountryIDAtInstall,
                           kBelgiumCountryId);

  const TemplateURL* default_search_engine =
      template_url_service->GetDefaultSearchProvider();
  SearchEngineType default_search_engine_type =
      default_search_engine->GetEngineType(
          template_url_service->search_terms_data());

  CHECK_NE(default_search_engine_type, SearchEngineType::SEARCH_ENGINE_BING);
  base::Value::List args;
  // Search engine model id.
  args.Append(1);
  args.Append(static_cast<int>(
      search_engines::ChoiceMadeLocation::kSearchEngineSettings));
  web_ui()->HandleReceivedMessage("setDefaultSearchEngine", args);

  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_BING, 1);
}
}  // namespace settings
