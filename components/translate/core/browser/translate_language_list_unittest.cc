// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_language_list.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_url_util.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace translate {

class TranslateLanguageListTest : public testing::Test {
 public:
 private:
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

// Test that the supported languages can be explicitly set using
// SetSupportedLanguages().
TEST_F(TranslateLanguageListTest, SetSupportedLanguages) {
  const std::string language_list(
      "{"
      "\"sl\":{\"en\":\"English\",\"ja\":\"Japanese\",\"tl\":\"Tagalog\","
      "\"xx\":\"NotALanguage\"},"
      "\"tl\":{\"en\":\"English\",\"ja\":\"Japanese\",\"tl\":\"Tagalog\","
      "\"xx\":\"NotALanguage\"}"
      "}");

  base::test::TaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);
  TranslateDownloadManager* manager = TranslateDownloadManager::GetInstance();
  manager->set_application_locale("en");
  manager->set_url_loader_factory(test_shared_loader_factory);
  EXPECT_TRUE(manager->language_list()->SetSupportedLanguages(language_list));

  std::vector<std::string> results;
  manager->language_list()->GetSupportedLanguages(true /* translate_allowed */,
                                                  &results);
  ASSERT_EQ(3u, results.size());
  EXPECT_EQ("en", results[0]);
  EXPECT_EQ("ja", results[1]);
  EXPECT_EQ("tl", results[2]);
  manager->ResetForTesting();
}

// Test that the language code back-off of locale is done correctly (where
// required).
TEST_F(TranslateLanguageListTest, GetLanguageCode) {
  TranslateLanguageList language_list;
  EXPECT_EQ("en", language_list.GetLanguageCode("en"));
  // Test backoff of unsupported locale.
  EXPECT_EQ("en", language_list.GetLanguageCode("en-US"));
  // Test supported locale not backed off.
  EXPECT_EQ("zh-CN", language_list.GetLanguageCode("zh-CN"));
}

// Test that the translation URL is correctly generated, and that the
// translate-security-origin command-line flag correctly overrides the default
// value.
TEST_F(TranslateLanguageListTest, TranslateLanguageUrl) {
  TranslateLanguageList language_list;

  // Test default security origin.
  // The command-line override switch should not be set by default.
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      "translate-security-origin"));
  EXPECT_EQ("https://translate.googleapis.com/translate_a/l?client=chrome",
            language_list.TranslateLanguageUrl().spec());

  // Test command-line security origin.
  base::test::ScopedCommandLine scoped_command_line;
  // Set the override switch.
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "translate-security-origin", "https://example.com");
  EXPECT_EQ("https://example.com/translate_a/l?client=chrome",
            language_list.TranslateLanguageUrl().spec());
}

// Test that IsSupportedLanguage() is true for languages that should be
// supported, and false for invalid languages.
TEST_F(TranslateLanguageListTest, IsSupportedLanguage) {
  TranslateLanguageList language_list;
  EXPECT_TRUE(language_list.IsSupportedLanguage("en"));
  EXPECT_TRUE(language_list.IsSupportedLanguage("zh-CN"));
  EXPECT_FALSE(language_list.IsSupportedLanguage("xx"));
}

// Test that IsSupportedPartialTranslateLanguage() is true for languages that
// should be supported, and false for invalid languages.
TEST_F(TranslateLanguageListTest, IsSupportedPartialTranslateLanguage) {
  TranslateLanguageList language_list;
  EXPECT_TRUE(language_list.IsSupportedPartialTranslateLanguage("en"));
  EXPECT_TRUE(language_list.IsSupportedPartialTranslateLanguage("zh-CN"));
  EXPECT_FALSE(language_list.IsSupportedPartialTranslateLanguage("xx"));
  EXPECT_FALSE(language_list.IsSupportedPartialTranslateLanguage("ilo"));
  EXPECT_FALSE(language_list.IsSupportedPartialTranslateLanguage("mni-Mtei"));
}

// Sanity test for the default set of supported languages. The default set of
// languages should be large (> 100) and must contain very common languages.
// If either of these tests are not true, the default language configuration is
// likely to be incorrect.
TEST_F(TranslateLanguageListTest, GetSupportedLanguages) {
  TranslateLanguageList language_list;
  std::vector<std::string> languages;
  language_list.GetSupportedLanguages(true /* translate_allowed */, &languages);
  // Check there are a lot of default languages.
  EXPECT_GE(languages.size(), 100ul);
  // Check that some very common languages are there.
  EXPECT_TRUE(base::Contains(languages, "en"));
  EXPECT_TRUE(base::Contains(languages, "es"));
  EXPECT_TRUE(base::Contains(languages, "fr"));
  EXPECT_TRUE(base::Contains(languages, "ru"));
  EXPECT_TRUE(base::Contains(languages, "zh-CN"));
  EXPECT_TRUE(base::Contains(languages, "zh-TW"));
}

// Sanity test for the default set of partial translate supported languages. The
// default set of languages should be large (> 100) and must contain very common
// languages. If either of these tests are not true, the default language
// configuration is likely to be incorrect.
TEST_F(TranslateLanguageListTest, GetSupportedPartialTranslateLanguages) {
  TranslateLanguageList language_list;
  std::vector<std::string> languages;
  language_list.GetSupportedPartialTranslateLanguages(&languages);
  // Check there are a lot of default languages.
  EXPECT_GE(languages.size(), 100ul);
  // Check that some very common languages are there.
  EXPECT_TRUE(base::Contains(languages, "en"));
  EXPECT_TRUE(base::Contains(languages, "es"));
  EXPECT_TRUE(base::Contains(languages, "fr"));
  EXPECT_TRUE(base::Contains(languages, "ru"));
  EXPECT_TRUE(base::Contains(languages, "zh-CN"));
  EXPECT_TRUE(base::Contains(languages, "zh-TW"));

  // Check that unsupported languages are not there
  EXPECT_FALSE(base::Contains(languages, "ilo"));
  EXPECT_FALSE(base::Contains(languages, "lus"));
  EXPECT_FALSE(base::Contains(languages, "mni-Mtei"));
  EXPECT_FALSE(base::Contains(languages, "gom"));
  EXPECT_FALSE(base::Contains(languages, "doi"));
  EXPECT_FALSE(base::Contains(languages, "bm"));
  EXPECT_FALSE(base::Contains(languages, "ckb"));
}

// Check that we contact the translate server to update the supported language
// list when translate is enabled by policy.
TEST_F(TranslateLanguageListTest, GetSupportedLanguagesFetch) {
  // Set up fake network environment.
  base::test::TaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);
  TranslateDownloadManager::GetInstance()->set_application_locale("en");
  TranslateDownloadManager::GetInstance()->set_url_loader_factory(
      test_shared_loader_factory);

  GURL actual_url;
  base::RunLoop loop;
  // Since translate is allowed by policy, we will schedule a language list
  // load. Intercept to ensure the URL is correct.
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        actual_url = request.url;
        loop.Quit();
      }));

  // Populate supported languages.
  std::vector<std::string> languages;
  TranslateLanguageList language_list;
  language_list.SetResourceRequestsAllowed(true);
  language_list.GetSupportedLanguages(true /* translate_allowed */, &languages);

  // Check that the correct URL is requested.
  const GURL expected_url =
      AddApiKeyToUrl(AddHostLocaleToUrl(language_list.TranslateLanguageUrl()));

  // Simulate fetch completion with just Italian in the supported language list.
  test_url_loader_factory.AddResponse(expected_url.spec(),
                                      R"({"tl" : {"it" : "Italian"}})");
  loop.Run();

  // Spin an extra loop so that we ensure the SimpleURLLoader fixture callback
  // is called.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(actual_url.is_valid());
  EXPECT_EQ(expected_url.spec(), actual_url.spec());

  // Check that the language list has been updated correctly.
  languages.clear();
  language_list.GetSupportedLanguages(true /* translate_allowed */, &languages);
  EXPECT_EQ(std::vector<std::string>(1, "it"), languages);

  TranslateDownloadManager::GetInstance()->ResetForTesting();
}

// Check that we don't send any network data when translate is disabled by
// policy.
TEST_F(TranslateLanguageListTest, GetSupportedLanguagesNoFetch) {
  // Set up fake network environment.
  base::test::TaskEnvironment task_environment;
  network::TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);
  TranslateDownloadManager::GetInstance()->set_application_locale("en");
  TranslateDownloadManager::GetInstance()->set_url_loader_factory(
      test_shared_loader_factory);

  // Populate supported languages.
  std::vector<std::string> languages;
  TranslateLanguageList language_list;
  language_list.SetResourceRequestsAllowed(true);
  language_list.GetSupportedLanguages(false /* translate_allowed */,
                                      &languages);

  // Since translate is disabled by policy, we should *not* have scheduled a
  // language list load.
  EXPECT_FALSE(language_list.HasOngoingLanguageListLoadingForTesting());
  EXPECT_TRUE(test_url_loader_factory.pending_requests()->empty());

  TranslateDownloadManager::GetInstance()->ResetForTesting();
}

}  // namespace translate
