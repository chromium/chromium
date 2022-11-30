// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ExcludeMatchesManifestTest : public ChromeManifestTest {
};

TEST_F(ExcludeMatchesManifestTest, ExcludeMatchPatterns) {
  Testcase testcases[] = {
    Testcase("exclude_matches.json"),
    Testcase("exclude_matches_empty.json")
  };
  RunTestcases(testcases, std::size(testcases), EXPECT_TYPE_SUCCESS);

  Testcase testcases2[] = {
      Testcase("exclude_matches_not_list.json",
               "Error at key 'content_scripts'. Parsing array failed at index "
               "0: 'exclude_matches': expected list, got string"),
      Testcase("exclude_matches_invalid_host.json",
               "Invalid value for 'content_scripts[0].exclude_matches[0]': "
               "Invalid host wildcard.")};
  RunTestcases(testcases2, std::size(testcases2), EXPECT_TYPE_ERROR);
}

}  // namespace extensions
