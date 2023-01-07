// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/accept_languages_service.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using translate::TranslateDownloadManager;

namespace language {
namespace {

// RAII class to set the TranslateDownloadManager application locale, and then
// restore it to the original value when the object goes out of scope.
class TranslateLocaleRestorer {
 public:
  explicit TranslateLocaleRestorer(const std::string& new_locale)
      : existing_locale_(
            TranslateDownloadManager::GetInstance()->application_locale()) {
    TranslateDownloadManager::GetInstance()->set_application_locale(new_locale);
  }
  ~TranslateLocaleRestorer() {
    TranslateDownloadManager::GetInstance()->set_application_locale(
        existing_locale_);
  }

 private:
  const std::string existing_locale_;
};

TEST(AcceptLanguagesServiceTest, TestIsAcceptLanguage) {
  const char* const pref_setting = "translate-accept-languages";
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterStringPref(pref_setting, "en-US,es,zh-CN");
  AcceptLanguagesService accept_languages(&prefs, pref_setting);

  // All valid.
  EXPECT_TRUE(accept_languages.IsAcceptLanguage("en"));
  EXPECT_TRUE(accept_languages.IsAcceptLanguage("en-US"));
  EXPECT_TRUE(accept_languages.IsAcceptLanguage("es"));
  EXPECT_TRUE(accept_languages.IsAcceptLanguage("zh-CN"));

  // Not valid language.
  EXPECT_FALSE(accept_languages.IsAcceptLanguage("xx"));

  // zh-XX cannot be shortened to zh.
  EXPECT_FALSE(accept_languages.IsAcceptLanguage("zh"));
}

TEST(AcceptLanguagesServiceTest, TestCanBeAcceptLanguage) {
  TranslateLocaleRestorer locale_restorer("es");

  // Valid accept languages.
  EXPECT_TRUE(AcceptLanguagesService::CanBeAcceptLanguage("en"));
  EXPECT_TRUE(AcceptLanguagesService::CanBeAcceptLanguage("en-US"));
  EXPECT_TRUE(AcceptLanguagesService::CanBeAcceptLanguage("es"));
  EXPECT_TRUE(AcceptLanguagesService::CanBeAcceptLanguage("es-419"));
  EXPECT_TRUE(AcceptLanguagesService::CanBeAcceptLanguage("zh-CN"));

  // Not valid format.
  EXPECT_FALSE(AcceptLanguagesService::CanBeAcceptLanguage("en-us"));
  EXPECT_FALSE(AcceptLanguagesService::CanBeAcceptLanguage("zh-Hant"));

  // Not valid language.
  EXPECT_FALSE(AcceptLanguagesService::CanBeAcceptLanguage("xx"));
}

}  // namespace
}  // namespace language
