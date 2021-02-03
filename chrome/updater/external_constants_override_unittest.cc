// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/external_constants_override.h"
#include "chrome/updater/updater_branding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace updater {

class ExternalConstantsOverriderTest : public ::testing::Test {};

TEST_F(ExternalConstantsOverriderTest, TestEmptyDictValue) {
  ExternalConstantsOverrider overrider(
      base::flat_map<std::string, base::Value>{},
      CreateDefaultExternalConstantsForTesting());

  EXPECT_TRUE(overrider.UseCUP());

  std::vector<GURL> urls = overrider.UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL(UPDATE_CHECK_URL));
  EXPECT_TRUE(urls[0].is_valid());

  EXPECT_EQ(overrider.InitialDelay(), kInitialDelay);
  EXPECT_EQ(overrider.ServerKeepAliveSeconds(), kServerKeepAliveSeconds);
}

TEST_F(ExternalConstantsOverriderTest, TestFullOverrides) {
  base::Value::DictStorage overrides;
  base::Value::ListStorage url_list;
  url_list.push_back(base::Value("https://www.example.com"));
  url_list.push_back(base::Value("https://www.google.com"));
  overrides[kDevOverrideKeyUseCUP] = base::Value(false);
  overrides[kDevOverrideKeyUrl] = base::Value(std::move(url_list));
  overrides[kDevOverrideKeyInitialDelay] = base::Value(137.1);
  overrides[kDevOverrideKeyServerKeepAliveSeconds] = base::Value(1);
  ExternalConstantsOverrider overrider(
      std::move(overrides), CreateDefaultExternalConstantsForTesting());

  EXPECT_FALSE(overrider.UseCUP());

  std::vector<GURL> urls = overrider.UpdateURL();
  ASSERT_EQ(urls.size(), 2ul);
  EXPECT_EQ(urls[0], GURL("https://www.example.com"));
  EXPECT_TRUE(urls[0].is_valid());
  EXPECT_EQ(urls[1], GURL("https://www.google.com"));
  EXPECT_TRUE(urls[1].is_valid());

  EXPECT_EQ(overrider.InitialDelay(), 137.1);
  EXPECT_EQ(overrider.ServerKeepAliveSeconds(), 1);
}

TEST_F(ExternalConstantsOverriderTest, TestOverrideUnwrappedURL) {
  base::Value::DictStorage overrides;
  overrides[kDevOverrideKeyUrl] = base::Value("https://www.example.com");
  ExternalConstantsOverrider overrider(
      std::move(overrides), CreateDefaultExternalConstantsForTesting());

  std::vector<GURL> urls = overrider.UpdateURL();
  ASSERT_EQ(urls.size(), 1ul);
  EXPECT_EQ(urls[0], GURL("https://www.example.com"));
  EXPECT_TRUE(urls[0].is_valid());

  // Non-overridden items should fall back to defaults
  EXPECT_TRUE(overrider.UseCUP());
  EXPECT_EQ(overrider.InitialDelay(), kInitialDelay);
  EXPECT_EQ(overrider.ServerKeepAliveSeconds(), kServerKeepAliveSeconds);
}

}  // namespace updater
