// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/app_browser_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

// Tests that various URLs get formatted as an origin string in the desired way.
TEST(AppBrowserController, FormatUrlOrigin) {
  constexpr struct {
    const char* const input;
    const wchar_t* const expectation;
  } kTestCases[] = {
      // www and trailing / are dropped.
      {"https://www.test.com/", L"test.com"},
      // Non-origin components are dropped.
      {"https://www.test.com/with/path/to/file.html", L"test.com"},
      // http:// is dropped.
      {"http://www.insecure.com/", L"insecure.com"},
      // Subdomains other than www are preserved.
      {"https://subdomain.domain.com/", L"subdomain.domain.com"},
      // Punycode gets rendered as Unicode.
      {"https://xn--1lq90ic7f1rc.cn", L"\x5317\x4eac\x5927\x5b78.cn"},
      // Check that the block list is being applied, taken from more
      // comprehensive tests in
      // components/url_formatter/url_formatter_unittest.cc.
      {"https://xn--36c-tfa.com", L"xn--36c-tfa.com"},
  };
  for (auto test_case : kTestCases) {
    EXPECT_EQ(AppBrowserController::FormatUrlOrigin(GURL(test_case.input)),
              base::WideToUTF16(test_case.expectation));
  }
}

}  // namespace web_app
