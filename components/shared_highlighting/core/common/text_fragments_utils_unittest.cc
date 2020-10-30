// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/text_fragments_utils.h"

#include "components/shared_highlighting/core/common/text_fragment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace shared_highlighting {
namespace {

TEST(TextFragmentsUtilsTest, AppendFragmentDirectivesOneFragment) {
  GURL base_url("https://www.chromium.org");
  TextFragment test_fragment("only start");

  GURL created_url = AppendFragmentDirectives(base_url, {test_fragment});
  EXPECT_EQ("https://www.chromium.org/#:~:text=only%20start",
            created_url.spec());
}

TEST(TextFragmentsUtilsTest, AppendFragmentDirectivesURLWithPound) {
  GURL base_url("https://www.chromium.org/#");
  TextFragment test_fragment("only start");

  GURL created_url = AppendFragmentDirectives(base_url, {test_fragment});
  EXPECT_EQ("https://www.chromium.org/#:~:text=only%20start",
            created_url.spec());
}

TEST(TextFragmentsUtilsTest, AppendFragmentDirectivesURLWithPoundAndValue) {
  GURL base_url("https://www.chromium.org/#SomeAnchor");
  TextFragment test_fragment("only start");

  GURL created_url = AppendFragmentDirectives(base_url, {test_fragment});
  EXPECT_EQ("https://www.chromium.org/#SomeAnchor:~:text=only%20start",
            created_url.spec());
}

TEST(TextFragmentsUtilsTest,
     AppendFragmentDirectivesURLWithPoundAndExistingFragment) {
  GURL base_url("https://www.chromium.org/#SomeAnchor:~:text=some%20value");
  TextFragment test_fragment("only start");

  GURL created_url = AppendFragmentDirectives(base_url, {test_fragment});
  EXPECT_EQ(
      "https://www.chromium.org/"
      "#SomeAnchor:~:text=some%20value&text=only%20start",
      created_url.spec());
}

TEST(TextFragmentsUtilsTest, AppendFragmentDirectivesTwoFragments) {
  GURL base_url("https://www.chromium.org");
  TextFragment first_test_fragment("only start");
  TextFragment second_test_fragment("only,- start #2");

  GURL created_url = AppendFragmentDirectives(
      base_url, {first_test_fragment, second_test_fragment});
  EXPECT_EQ(
      "https://www.chromium.org/"
      "#:~:text=only%20start&text=only%2C%2D%20start%20%232",
      created_url.spec());
}

}  // namespace
}  // namespace shared_highlighting
