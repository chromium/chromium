// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/application_locale_storage/application_locale_storage.h"

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ApplicationLocaleStorageTest, GetShouldReturnPreviousSet) {
  ApplicationLocaleStorage locale_storage;
  locale_storage.Set("en-US");
  EXPECT_EQ(locale_storage.Get(), "en-US");
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
  }

  ASSERT_EQ(history.size(), static_cast<size_t>(1u));
  EXPECT_EQ(history[0], "ja");
}
