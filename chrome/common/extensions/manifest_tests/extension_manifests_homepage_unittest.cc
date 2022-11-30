// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_url_handlers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extensions::manifest_errors;

class HomepageURLManifestTest : public ChromeManifestTest {
};

TEST_F(HomepageURLManifestTest, ParseHomepageURLs) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("homepage_valid.json"));

  Testcase testcases[] = {
    Testcase("homepage_empty.json",
             errors::kInvalidHomepageURL),
    Testcase("homepage_invalid.json",
             errors::kInvalidHomepageURL),
    Testcase("homepage_bad_schema.json",
             errors::kInvalidHomepageURL)
  };
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(HomepageURLManifestTest, GetHomepageURL) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("homepage_valid.json"));
  EXPECT_EQ(GURL("http://foo.com#bar"),
            extensions::ManifestURL::GetHomepageURL(extension.get()));

  // The Google Gallery URL ends with the id, which depends on the path, which
  // can be different in testing, so we just check the part before id.
  extension = LoadAndExpectSuccess("homepage_google_hosted.json");
  EXPECT_TRUE(base::StartsWith(
      extensions::ManifestURL::GetHomepageURL(extension.get()).spec(),
      "https://chrome.google.com/webstore/detail/",
      base::CompareCase::INSENSITIVE_ASCII));

  extension = LoadAndExpectSuccess("homepage_externally_hosted.json");
  EXPECT_EQ(GURL(), extensions::ManifestURL::GetHomepageURL(extension.get()));
}
