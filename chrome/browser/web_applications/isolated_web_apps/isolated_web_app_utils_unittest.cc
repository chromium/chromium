// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_utils.h"
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {
constexpr char kValidIsolatedAppUrl[] =
    "isolated-app://"
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/";
}

class IsolatedAppUtils
    : public ::testing::TestWithParam<std::pair<std::string, bool>> {};

TEST_P(IsolatedAppUtils, ParseIsolatedAppUrl) {
  EXPECT_EQ(ParseIsolatedAppUrl(GURL(GetParam().first)).has_value(),
            GetParam().second);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedAppUtils,
    ::testing::Values(
        std::make_pair(kValidIsolatedAppUrl, true),
        std::make_pair("isolated-app://"
                       "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaai"
                       "c/?foo=bar#baz",
                       true),
        // Invalid scheme
        std::make_pair(
            "https://"
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/",
            false),
        // No scheme
        std::make_pair(
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/",
            false),
        // Invalid Signed Web Bundle ID
        std::make_pair(
            "isolated-app://"
            "ßerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/",
            false)));

class IsolatedAppGURLConversions
    : public ::testing::TestWithParam<std::pair<std::string, std::string>> {};

TEST_P(IsolatedAppGURLConversions, RemovesInvalidPartsFromUrls) {
  // GURL automatically removes port and credentials, and converts
  // `isolated-app:foo` to `isolated-app://foo`. This test is here to verify
  // that and therefore make sure that the `CHECK` inside `ParseIsolatedAppUrl`
  // will never actually trigger as long as this test succeeds.
  GURL gurl(GetParam().first);
  EXPECT_TRUE(gurl.IsStandard());
  EXPECT_EQ(gurl.spec(), GetParam().second);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IsolatedAppGURLConversions,
    ::testing::Values(
        std::make_pair(kValidIsolatedAppUrl, kValidIsolatedAppUrl),
        // credentials
        std::make_pair(
            "isolated-app://"
            "foo:bar@aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/",
            kValidIsolatedAppUrl),
        // explicit port
        std::make_pair(
            "isolated-app://"
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic:123/",
            kValidIsolatedAppUrl),
        // missing `//`
        std::make_pair(
            "isolated-app:"
            "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/",
            kValidIsolatedAppUrl)));

}  // namespace web_app
