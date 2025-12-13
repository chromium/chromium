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
