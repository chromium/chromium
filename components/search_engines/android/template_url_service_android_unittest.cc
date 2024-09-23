// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/android/template_url_service_android.h"

#include <string_view>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
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
    // Chosen due to being an EEA country, see
    // `search_engines::IsEeaChoiceCountry()`.
    const char kBelgiumCountryId[] = "BE";
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, kBelgiumCountryId);

    LoadedTemplateURLServiceUnitTestBase::SetUp();

    env_ = base::android::AttachCurrentThread();

    template_url_service_android_ =
        std::make_unique<TemplateUrlServiceAndroid>(&template_url_service());
  }

  base::android::JavaParamRef<jstring> ToParamRef(
      base::android::ScopedJavaLocalRef<jstring> local_ref) {
    return base::android::JavaParamRef<jstring>(env_, local_ref.obj());
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
      env(), base::android::JavaParamRef<jobject>(nullptr),
      ToParamRef(short_name), ToParamRef(jkeyword), ToParamRef(search_url),
      ToParamRef(suggest_url), ToParamRef(favicon_url), ToParamRef(new_tab_url),
      ToParamRef(image_url), ToParamRef(image_url_post_params),
      ToParamRef(image_translate_url),
      ToParamRef(image_translate_source_language_param_key),
      ToParamRef(image_translate_target_language_param_key));

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
