// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_fetcher.h"
#include "chrome/common/chrome_version.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(WhatsNewUtil, GetServerURL) {
  const std::string expected_no_redirect = base::StringPrintf(
      "https://www.google.com/chrome/whats-new/m%d?internal=true",
      CHROME_VERSION_MAJOR);
  const std::string expected_redirect = base::StringPrintf(
      "https://www.google.com/chrome/whats-new/?version=%d&internal=true",
      CHROME_VERSION_MAJOR);
  const std::string expected_staging_redirect = base::StringPrintf(
      "https://chrome-staging.corp.google.com/chrome/whats-new/"
      "?version=%d&internal=true",
      CHROME_VERSION_MAJOR);

  EXPECT_EQ(expected_no_redirect,
            whats_new::GetServerURL(false).possibly_invalid_spec());
  EXPECT_EQ(expected_redirect,
            whats_new::GetServerURL(true).possibly_invalid_spec());
  EXPECT_EQ(expected_staging_redirect,
            whats_new::GetServerURL(true, true).possibly_invalid_spec());
}
