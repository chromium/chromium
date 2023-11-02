// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST_F(ChromeManifestTest, DefaultPathForExtent) {
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("default_path_for_extent.json"));

  ASSERT_EQ(1u, extension->web_extent().patterns().size());
  EXPECT_EQ("/*", extension->web_extent().patterns().begin()->path());
  EXPECT_TRUE(extension->web_extent().MatchesURL(
      GURL("http://www.google.com/monkey")));
}
