// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_ui_utils.h"

#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace password_manager {

TEST(GetShownOriginTest, RemovePrefixes) {
  const struct {
    const std::string input;
    const std::string output;
  } kTestCases[] = {
      {"http://subdomain.example.com:80/login/index.html?h=90&ind=hello#first",
       "subdomain.example.com"},
      {"https://www.example.com", "example.com"},
      {"https://mobile.example.com", "example.com"},
      {"https://m.example.com", "example.com"},
      {"https://m.subdomain.example.net", "subdomain.example.net"},
      {"https://mobile.de", "mobile.de"},
      {"https://www.de", "www.de"},
      {"https://m.de", "m.de"},
      {"https://www.mobile.de", "mobile.de"},
      {"https://m.mobile.de", "mobile.de"},
      {"https://m.www.de", "www.de"},
      {"https://Mobile.example.de", "example.de"},
      {"https://WWW.Example.DE", "example.de"}};

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.output,
              GetShownOrigin(url::Origin::Create(GURL(test_case.input))))
        << "for input " << test_case.input;
  }
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromAndroidForm_NoAppDisplayName) {
  PasswordForm android_form;
  android_form.signon_realm = "android://hash@com.example.android";
  android_form.app_display_name.clear();

  auto shown_origin = GetShownOrigin(CredentialUIEntry(android_form));
  auto shown_url = GetShownUrl(CredentialUIEntry(android_form));

  EXPECT_EQ("android.example.com", shown_origin);
  EXPECT_EQ("https://play.google.com/store/apps/details?id=com.example.android",
            shown_url.spec());
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromAndroidForm_WithAppDisplayName) {
  PasswordForm android_form;
  android_form.signon_realm = "android://hash@com.example.android";
  android_form.app_display_name = "Example Android App";

  auto shown_origin = GetShownOrigin(CredentialUIEntry(android_form));
  auto shown_url = GetShownUrl(CredentialUIEntry(android_form));

  EXPECT_EQ("Example Android App", shown_origin);
  EXPECT_EQ("https://play.google.com/store/apps/details?id=com.example.android",
            shown_url.spec());
}

TEST(GetShownOriginAndLinkUrlTest, OriginFromNonAndroidForm) {
  PasswordForm form;
  form.signon_realm = "https://example.com/";
  form.url = GURL("https://example.com/login?ref=1");

  auto shown_origin = GetShownOrigin(CredentialUIEntry(form));
  auto shown_url = GetShownUrl(CredentialUIEntry(form));

  EXPECT_EQ("example.com", shown_origin);
  EXPECT_EQ(GURL("https://example.com/login?ref=1"), shown_url);
}

TEST(ToUsernameString, NonEmptyUsername) {
  EXPECT_EQ(ToUsernameString("nadeshiko"), u"nadeshiko");
}

TEST(ToUsernameString, EmptyUsername) {
  EXPECT_EQ(ToUsernameString(""),
            l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN));
}

}  // namespace password_manager
