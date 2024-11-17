// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_languages_controller.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lens/core/mojom/translate.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {

constexpr char kExampleJsonResponse[] = R"JSON(
  {
    "sourceLanguages": [
      {
        "language": "auto",
        "name": "Detect language"
      },
      {
        "language": "en",
        "name": "English"
      }
    ],
    "targetLanguages": [
      {
        "language": "en",
        "name": "English"
      },
      {
        "language": "es",
        "name": "Spanish"
      }
    ]
  }
)JSON";
constexpr char kExampleIncorrectJsonResponse[] = R"JSON(
[
  {
    "language": "auto",
    "name": "Detect language"
  },
  {
    "language": "en",
    "name": "English"
  }
]
)JSON";

class LensOverlayLanguagesControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    profile_ = profile_builder.Build();

    g_browser_process->SetApplicationLocale("en-US");
  }

  TestingProfile* profile() { return profile_.get(); }

  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::vector<lens::mojom::LanguagePtr> sent_languages_;
};

TEST_F(LensOverlayLanguagesControllerTest,
       SendGetSupportedLanguagesRequest_SuccessfulResponse) {
  auto languages_controller =
      std::make_unique<LensOverlayLanguagesController>(profile());
  std::string locale;
  std::vector<mojom::LanguagePtr> sent_source_languages;
  std::vector<mojom::LanguagePtr> sent_target_languages;

  base::RunLoop run_loop;
  languages_controller->SendGetSupportedLanguagesRequest(
      base::BindLambdaForTesting(
          [&](const std::string& browser_locale,
              std::vector<mojom::LanguagePtr> source_languages,
              std::vector<mojom::LanguagePtr> target_languages) {
            locale = browser_locale;
            sent_source_languages = std::move(source_languages);
            sent_target_languages = std::move(target_languages);
            run_loop.Quit();
          }));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      BuildTranslateLanguagesURL("US", "en").spec(), kExampleJsonResponse);
  run_loop.Run();

  EXPECT_EQ(locale, "en-US");

  EXPECT_EQ(sent_source_languages.size(), 2UL);
  const auto& detect_language = sent_source_languages.at(0);
  EXPECT_EQ(detect_language->language_code, "auto");
  EXPECT_EQ(detect_language->name, "Detect language");
  const auto& english_language = sent_source_languages.at(1);
  EXPECT_EQ(english_language->language_code, "en");
  EXPECT_EQ(english_language->name, "English");

  EXPECT_EQ(sent_target_languages.size(), 2UL);
  const auto& english_target = sent_target_languages.at(0);
  EXPECT_EQ(english_target->language_code, "en");
  EXPECT_EQ(english_target->name, "English");
  const auto& spanish_target = sent_target_languages.at(1);
  EXPECT_EQ(spanish_target->language_code, "es");
  EXPECT_EQ(spanish_target->name, "Spanish");
}

TEST_F(LensOverlayLanguagesControllerTest,
       SendGetSupportedLanguagesRequest_RequestTimeOut) {
  auto languages_controller =
      std::make_unique<LensOverlayLanguagesController>(profile());
  std::string locale;
  std::vector<mojom::LanguagePtr> sent_source_languages;
  std::vector<mojom::LanguagePtr> sent_target_languages;

  base::RunLoop run_loop;
  languages_controller->SendGetSupportedLanguagesRequest(
      base::BindLambdaForTesting(
          [&](const std::string& browser_locale,
              std::vector<mojom::LanguagePtr> source_languages,
              std::vector<mojom::LanguagePtr> target_languages) {
            locale = browser_locale;
            sent_source_languages = std::move(source_languages);
            sent_target_languages = std::move(target_languages);
            run_loop.Quit();
          }));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      BuildTranslateLanguagesURL("US", "en").spec(), "",
      net::HTTP_REQUEST_TIMEOUT);
  run_loop.Run();

  EXPECT_EQ(locale, "en-US");
  EXPECT_EQ(sent_source_languages.size(), 0UL);
  EXPECT_EQ(sent_target_languages.size(), 0UL);
}

TEST_F(LensOverlayLanguagesControllerTest,
       SendGetSupportedLanguagesRequest_EmptyResponse) {
  auto languages_controller =
      std::make_unique<LensOverlayLanguagesController>(profile());
  std::string locale;
  std::vector<mojom::LanguagePtr> sent_source_languages;
  std::vector<mojom::LanguagePtr> sent_target_languages;

  base::RunLoop run_loop;
  languages_controller->SendGetSupportedLanguagesRequest(
      base::BindLambdaForTesting(
          [&](const std::string& browser_locale,
              std::vector<mojom::LanguagePtr> source_languages,
              std::vector<mojom::LanguagePtr> target_languages) {
            locale = browser_locale;
            sent_source_languages = std::move(source_languages);
            sent_target_languages = std::move(target_languages);
            run_loop.Quit();
          }));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      BuildTranslateLanguagesURL("US", "en").spec(), "");
  run_loop.Run();

  EXPECT_EQ(locale, "en-US");
  EXPECT_EQ(sent_source_languages.size(), 0UL);
  EXPECT_EQ(sent_target_languages.size(), 0UL);
}

TEST_F(LensOverlayLanguagesControllerTest,
       SendGetSupportedLanguagesRequest_IncorrectResponse) {
  auto languages_controller =
      std::make_unique<LensOverlayLanguagesController>(profile());
  std::string locale;
  std::vector<mojom::LanguagePtr> sent_source_languages;
  std::vector<mojom::LanguagePtr> sent_target_languages;

  base::RunLoop run_loop;
  languages_controller->SendGetSupportedLanguagesRequest(
      base::BindLambdaForTesting(
          [&](const std::string& browser_locale,
              std::vector<mojom::LanguagePtr> source_languages,
              std::vector<mojom::LanguagePtr> target_languages) {
            locale = browser_locale;
            sent_source_languages = std::move(source_languages);
            sent_target_languages = std::move(target_languages);
            run_loop.Quit();
          }));
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      BuildTranslateLanguagesURL("US", "en").spec(),
      kExampleIncorrectJsonResponse);
  run_loop.Run();

  EXPECT_EQ(locale, "en-US");
  EXPECT_EQ(sent_source_languages.size(), 0UL);
  EXPECT_EQ(sent_target_languages.size(), 0UL);
}

}  // namespace lens
