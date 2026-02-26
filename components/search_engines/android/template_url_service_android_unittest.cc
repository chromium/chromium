// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/android/template_url_service_android.h"

#include <string_view>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/template_url_service_test_util.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

class TemplateUrlServiceAndroidUnitTest
    : public LoadedTemplateURLServiceUnitTestBase {
 public:
  void SetUp() override {
    // Chosen due to being associated to
    // `regional_capabilities::ProgramSettings::kWaffle`.
    const char kBelgiumCountryId[] = "BE";
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, kBelgiumCountryId);

    LoadedTemplateURLServiceUnitTestBase::SetUp();

    env_ = base::android::AttachCurrentThread();

    template_url_service_android_ =
        std::make_unique<TemplateUrlServiceAndroid>(&template_url_service());
  }

  base::android::ScopedJavaLocalRef<jstring> ToLocalJavaString(
      std::string_view str) {
    return base::android::ConvertUTF8ToJavaString(env_, str);
  }

  base::android::ScopedJavaLocalRef<jstring> ToLocalJavaString(
      std::u16string_view str) {
    return base::android::ConvertUTF16ToJavaString(env_, str);
  }

  JNIEnv* env() { return env_; }

  TemplateUrlServiceAndroid& template_url_service_android() {
    return *template_url_service_android_.get();
  }

 private:
  raw_ptr<JNIEnv> env_ = nullptr;

  std::unique_ptr<TemplateUrlServiceAndroid> template_url_service_android_;
};

TEST_F(TemplateUrlServiceAndroidUnitTest, SetPlayAPISearchEngine) {
  base::HistogramTester histogram_tester;
  const std::u16string keyword = u"chromium";

  auto short_name = ToLocalJavaString(u"Chromium Search");
  auto jkeyword = ToLocalJavaString(keyword);
  auto search_url =
      ToLocalJavaString("http://chromium.org/search?q={searchTerms}");
  auto suggest_url = ToLocalJavaString("http://chromium.org/search/suggest");
  auto favicon_url = ToLocalJavaString("http://chromium.org/search/favicon");
  auto new_tab_url = ToLocalJavaString("https://chromium.org/search/newtab");
  auto image_url = ToLocalJavaString("https://chromium.org/search/img");
  auto image_url_post_params = ToLocalJavaString("param");
  auto image_translate_url =
      ToLocalJavaString("https://chromium.org/search/transl");
  auto image_translate_source_language_param_key = ToLocalJavaString("s");
  auto image_translate_target_language_param_key = ToLocalJavaString("t");

  ASSERT_NE(template_url_service().GetDefaultSearchProvider()->keyword(),
            keyword);

  template_url_service_android().SetPlayAPISearchEngine(
      env(), short_name, jkeyword, search_url, suggest_url, favicon_url,
      new_tab_url, image_url, image_url_post_params, image_translate_url,
      image_translate_source_language_param_key,
      image_translate_target_language_param_key);

  TemplateURL* t_url = template_url_service().GetTemplateURLForKeyword(keyword);
  EXPECT_TRUE(t_url);
  EXPECT_EQ(template_url_service().GetDefaultSearchProvider()->keyword(),
            keyword);
  EXPECT_EQ(t_url->GetEngineType(SearchTermsData()),
            SearchEngineType::SEARCH_ENGINE_OTHER);

  EXPECT_TRUE(pref_service().HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  histogram_tester.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);
}

TEST_F(TemplateUrlServiceAndroidUnitTest, RemoveSearchEngine) {
  const std::u16string keyword = u"chromium";
  TemplateURLData data;
  data.SetShortName(u"Delete Test");
  data.SetKeyword(keyword);
  data.SetURL("http://chromium.org/search/delete");
  template_url_service().Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url_service().GetTemplateURLForKeyword(keyword));

  EXPECT_TRUE(
      template_url_service_android().RemoveSearchEngine(env(), keyword));
  EXPECT_FALSE(template_url_service().GetTemplateURLForKeyword(keyword));
}

TEST_F(TemplateUrlServiceAndroidUnitTest,
       DeleteSearchEngineFailed_DefaultProvider) {
  const TemplateURL* default_provider =
      template_url_service().GetDefaultSearchProvider();
  ASSERT_TRUE(default_provider);

  std::u16string default_keyword = default_provider->keyword();
  // Delete search engine failed since it is the default search engine.
  EXPECT_FALSE(template_url_service_android().RemoveSearchEngine(
      env(), default_keyword));
  EXPECT_TRUE(template_url_service().GetTemplateURLForKeyword(default_keyword));
}

TEST_F(TemplateUrlServiceAndroidUnitTest, EditSearchEngine) {
  const std::u16string keyword = u"chromium";
  TemplateURLData data;
  data.SetShortName(u"Edit Test");
  data.SetKeyword(keyword);
  data.SetURL("http://chromium.org/search/edit");
  template_url_service().Add(std::make_unique<TemplateURL>(data));

  ASSERT_TRUE(template_url_service().GetTemplateURLForKeyword(keyword));

  const std::u16string new_short_name = u"New Edit Test";
  const std::u16string new_keyword = u"new_chromium";
  const std::string new_url = "http://chromium.org/search/edit_new";

  EXPECT_TRUE(template_url_service_android().EditSearchEngine(
      env(), keyword, new_short_name, new_keyword, new_url));

  TemplateURL* t_url =
      template_url_service().GetTemplateURLForKeyword(new_keyword);
  ASSERT_TRUE(t_url);
  EXPECT_EQ(t_url->short_name(), new_short_name);
  EXPECT_EQ(t_url->url(), new_url);

  EXPECT_FALSE(template_url_service().GetTemplateURLForKeyword(keyword));
}

TEST_F(TemplateUrlServiceAndroidUnitTest, AddSearchEngine) {
  const std::u16string keyword = u"chromium";
  const std::u16string short_name = u"Add Test";
  const std::string search_url = "http://chromium.org/search/add";

  EXPECT_TRUE(template_url_service_android().AddSearchEngine(
      env(), short_name, keyword, search_url));

  TemplateURL* t_url = template_url_service().GetTemplateURLForKeyword(keyword);
  ASSERT_TRUE(t_url);
  EXPECT_EQ(t_url->short_name(), short_name);
  EXPECT_EQ(t_url->url(), search_url);
  EXPECT_FALSE(t_url->safe_for_autoreplace());
}

TEST_F(TemplateUrlServiceAndroidUnitTest, AddSearchEngineFailed_KeywordExists) {
  const std::u16string keyword = u"chromium";
  TemplateURLData data;
  data.SetShortName(u"Existing");
  data.SetKeyword(keyword);
  data.SetURL("http://chromium.org/search/existing");
  template_url_service().Add(std::make_unique<TemplateURL>(data));

  const std::u16string short_name = u"Existing2";
  const std::string search_url = "http://chromium.org/search/add";

  EXPECT_FALSE(template_url_service_android().AddSearchEngine(
      env(), short_name, keyword, search_url));
}

TEST_F(TemplateUrlServiceAndroidUnitTest,
       EditSearchEngineFailed_PrepopulatedEngine) {
  const std::u16string keyword = u"chromium";
  TemplateURLData data;
  data.SetShortName(u"Edit Test");
  data.SetKeyword(keyword);
  data.SetURL("http://chromium.org/search/edit");
  data.prepopulate_id = 1;
  template_url_service().Add(std::make_unique<TemplateURL>(data));

  ASSERT_TRUE(template_url_service().GetTemplateURLForKeyword(keyword));

  const std::u16string new_short_name = u"New Edit Test";
  const std::u16string new_keyword = u"new_chromium";
  const std::string new_url = "http://chromium.org/search/edit_new";

  // EditSearchEngine should fail if we try to edit the url of a prepopulated
  // engine.
  EXPECT_FALSE(template_url_service_android().EditSearchEngine(
      env(), keyword, new_short_name, new_keyword, new_url));
}

TEST_F(TemplateUrlServiceAndroidUnitTest, FilterUserSelectableTemplateUrls) {
  struct InputEngine {
    std::u16string_view keyword;
    int starter_pack_id;
  };

  struct TestCase {
    std::string_view title;
    std::vector<InputEngine> input_data;
    std::set<std::u16string_view> expected_output;
  } test_cases[]{{
                     "No search engines",
                     {},
                     {},
                 },

                 {
                     "Conventional search engines only",
                     {{u"engine1", 0}, {u"engine2", 0}},
                     {u"engine1", u"engine2"},
                 },

                 {
                     "List with StarterPack engines",
                     {{u"engine1", 0},
                      {u"starterpack1", 1},
                      {u"engine2", 0},
                      {u"starterpack2", 2}},
                     {u"engine1", u"engine2"},
                 }};

  auto run_test = [](const TestCase& tc) {
    SCOPED_TRACE(tc.title);
    std::vector<TemplateURLData> tdata(tc.input_data.size());
    std::vector<std::unique_ptr<TemplateURL>> turls;
    std::vector<raw_ptr<TemplateURL, VectorExperimental>> input_data;
    for (size_t index = 0; index < tc.input_data.size(); ++index) {
      TemplateURLData& data = tdata[index];
      data.SetKeyword(tc.input_data[index].keyword);
      data.starter_pack_id = tc.input_data[index].starter_pack_id;
      turls.emplace_back(
          std::make_unique<TemplateURL>(data, TemplateURL::Type::NORMAL));
      input_data.push_back(turls.back().get());
    }

    auto result =
        TemplateUrlServiceAndroid::FilterUserSelectableTemplateUrls(input_data);
    // input_data is no longer needed; values retained in turls vector.
    input_data.clear();

    ASSERT_EQ(result.size(), tc.expected_output.size());
    for (TemplateURL* turl : result) {
      ASSERT_TRUE(tc.expected_output.contains(turl->keyword()))
          << "Unexpected engine: " << base::UTF16ToUTF8(turl->keyword());
    }
  };

  for (const auto& test_case : test_cases) {
    run_test(test_case);
  }
}

TEST_F(TemplateUrlServiceAndroidUnitTest, FilterTemplateUrlsByCategory) {
  using Category = TemplateUrlServiceAndroid::TemplateUrlCategory;

  TemplateURLData dse_data;
  dse_data.SetShortName(u"dse");
  dse_data.SetKeyword(u"dse");
  dse_data.SetURL("http://dse.com/q={searchTerms}");
  dse_data.is_active = TemplateURLData::ActiveStatus::kTrue;
  dse_data.safe_for_autoreplace = false;
  TemplateURL* t_dse =
      template_url_service().Add(std::make_unique<TemplateURL>(dse_data));
  template_url_service().SetUserSelectedDefaultSearchProvider(t_dse);

  TemplateURLData prepop_data;
  prepop_data.SetShortName(u"prepop");
  prepop_data.SetKeyword(u"prepop");
  prepop_data.SetURL("http://prepop.com/q={searchTerms}");
  prepop_data.prepopulate_id = 1;
  TemplateURL* t_prepop =
      template_url_service().Add(std::make_unique<TemplateURL>(prepop_data));

  TemplateURLData active_data;
  active_data.SetShortName(u"active");
  active_data.SetKeyword(u"active");
  active_data.SetURL("http://active.com/q={searchTerms}");
  active_data.is_active = TemplateURLData::ActiveStatus::kTrue;
  active_data.safe_for_autoreplace = false;
  TemplateURL* t_active =
      template_url_service().Add(std::make_unique<TemplateURL>(active_data));

  TemplateURLData inactive_data;
  inactive_data.SetShortName(u"inactive");
  inactive_data.SetKeyword(u"inactive");
  inactive_data.SetURL("http://inactive.com/q={searchTerms}");
  inactive_data.is_active = TemplateURLData::ActiveStatus::kFalse;
  inactive_data.safe_for_autoreplace = false;
  TemplateURL* t_inactive =
      template_url_service().Add(std::make_unique<TemplateURL>(inactive_data));

  TemplateURLData extension_data;
  extension_data.SetShortName(u"extension");
  extension_data.SetKeyword(u"extension");
  extension_data.SetURL("http://extension.com/q={searchTerms}");
  TemplateURL* t_extension =
      template_url_service().Add(std::make_unique<TemplateURL>(
          extension_data, TemplateURL::OMNIBOX_API_EXTENSION));

  // Create two engines with the same keyword. One will be hidden.
  TemplateURLData visible_data;
  visible_data.SetShortName(u"visible");
  visible_data.SetKeyword(u"duplicate");
  visible_data.SetURL("http://visible.com/q={searchTerms}");
  visible_data.safe_for_autoreplace = false;
  TemplateURL* t_visible =
      template_url_service().Add(std::make_unique<TemplateURL>(visible_data));

  TemplateURLData hidden_data;
  hidden_data.SetShortName(u"hidden");
  hidden_data.SetKeyword(u"duplicate");
  hidden_data.SetURL("http://hidden.com/q={searchTerms}");
  hidden_data.safe_for_autoreplace = false;
  TemplateURL* t_hidden_candidate =
      template_url_service().Add(std::make_unique<TemplateURL>(hidden_data));
  TemplateURL* t_best =
      template_url_service().GetTemplateURLForKeyword(u"duplicate");

  TemplateURL* t_hidden =
      (t_best == t_visible) ? t_hidden_candidate : t_visible;

  std::vector<raw_ptr<TemplateURL, VectorExperimental>> input_urls;
  input_urls.push_back(t_dse);
  input_urls.push_back(t_prepop);
  input_urls.push_back(t_active);
  input_urls.push_back(t_inactive);
  input_urls.push_back(t_extension);
  input_urls.push_back(t_hidden);

  {
    auto result = template_url_service_android().FilterTemplateUrlsByCategory(
        input_urls, Category::kDefault);
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], t_dse);
    EXPECT_EQ(result[1], t_prepop);
  }

  {
    auto result = template_url_service_android().FilterTemplateUrlsByCategory(
        input_urls, Category::kActiveSiteSearch);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], t_active);
  }

  {
    auto result = template_url_service_android().FilterTemplateUrlsByCategory(
        input_urls, Category::kInactiveSiteSearch);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], t_inactive);
  }

  {
    auto result = template_url_service_android().FilterTemplateUrlsByCategory(
        input_urls, Category::kExtension);
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], t_extension);
  }
}
