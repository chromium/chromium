// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/credential_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

TEST(CredentialUtils, IsValidPasswordURL) {
  std::vector<std::pair<GURL, bool>> test_cases = {
      {GURL("noscheme.com"), false},
      {GURL("https://;/valid"), true},
      {GURL("https://^/invalid"), false},
      {GURL("scheme://unsupported"), false},
      {GURL("http://example.com"), true},
      {GURL("https://test.com/login"), true},
      {GURL("android://certificate_hash@com.test.client/"), true}};
  for (const auto& test_case : test_cases) {
    EXPECT_EQ(test_case.second, IsValidPasswordURL(test_case.first));
  }
}

}  // namespace password_manager
