// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/application_locale_storage/application_locale_storage.h"

#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/i18n/language_tag.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ApplicationLocaleStorageTest, GetTagShouldReturnPreviousSetTag) {
  ApplicationLocaleStorage locale_storage;
  locale_storage.SetTag(base::i18n::GetKnownLanguageTag("pt-BR"));
  EXPECT_EQ(locale_storage.GetTag(), base::i18n::GetKnownLanguageTag("pt-BR"));
}

TEST(ApplicationLocaleStorageTest, SetTagShouldTriggerCallback) {
  std::vector<base::i18n::LanguageTag> history;
  base::RepeatingCallback callback = base::BindLambdaForTesting(
      [&](const base::i18n::LanguageTag& new_locale) {
        history.push_back(new_locale);
      });

  ApplicationLocaleStorage locale_storage;
  {
    auto subscription =
        locale_storage.RegisterOnLocaleChangedCallback(std::move(callback));
    locale_storage.SetTag(base::i18n::GetKnownLanguageTag("ja"));
    locale_storage.SetTag(base::i18n::GetKnownLanguageTag("fr"));
  }

  ASSERT_EQ(history.size(), static_cast<size_t>(2u));
  EXPECT_EQ(history[0], base::i18n::GetKnownLanguageTag("ja"));
  EXPECT_EQ(history[1], base::i18n::GetKnownLanguageTag("fr"));
}
