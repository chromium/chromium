// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/application_locale_storage/application_locale_storage.h"

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/i18n/language_tag.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ApplicationLocaleStorageTest, GetShouldReturnPreviousSet) {
  ApplicationLocaleStorage locale_storage;
  locale_storage.Set("en-US");
  EXPECT_EQ(locale_storage.Get(), "en-US");
  EXPECT_EQ(locale_storage.GetTag(), base::i18n::GetKnownLanguageTag("en-US"));
}

TEST(ApplicationLocaleStorageTest, GetTagShouldReturnPreviousSetTag) {
  ApplicationLocaleStorage locale_storage;
  locale_storage.SetTag(base::i18n::GetKnownLanguageTag("pt-BR"));
  EXPECT_EQ(locale_storage.GetTag(), base::i18n::GetKnownLanguageTag("pt-BR"));
  EXPECT_EQ(locale_storage.Get(), "pt-BR");
}

TEST(ApplicationLocaleStorageTest, GetBCP47Format) {
  ApplicationLocaleStorage locale_storage;
  locale_storage.Set("es_419");
  EXPECT_EQ(locale_storage.Get(
                ApplicationLocaleStorage::LocaleFormat::kChromeNormalized),
            "es-419");
  EXPECT_EQ(locale_storage.Get(ApplicationLocaleStorage::LocaleFormat::kBCP47),
            "es-419");
}

TEST(ApplicationLocaleStorageTest, SetShouldTriggerCallback) {
  std::vector<std::string> history;
  base::RepeatingCallback callback = base::BindLambdaForTesting(
      [&](const std::string& new_locale) { history.push_back(new_locale); });

  ApplicationLocaleStorage locale_storage;
  {
    auto subscription =
        locale_storage.RegisterOnLocaleChangedCallback(std::move(callback));
    locale_storage.Set("ja");
    locale_storage.SetTag(base::i18n::GetKnownLanguageTag("fr"));
  }

  ASSERT_EQ(history.size(), static_cast<size_t>(2u));
  EXPECT_EQ(history[0], "ja");
  EXPECT_EQ(history[1], "fr");
}
