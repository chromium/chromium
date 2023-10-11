// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/ui_utils.h"

#include <stddef.h>

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace {

// ScopedResourceOverride allows overriding localised strings in the shared
// instance of the resource bundle, while restoring the bundle state on
// destruction.
class ScopedResourceOverride {
 public:
  ScopedResourceOverride()
      : had_shared_instance_(ui::ResourceBundle::HasSharedInstance()),
        bundle_(GetOrCreateSharedInstance()),
        app_locale_(g_browser_process->GetApplicationLocale()) {}

  ScopedResourceOverride(const ScopedResourceOverride&) = delete;
  ScopedResourceOverride& operator=(const ScopedResourceOverride&) = delete;

  ~ScopedResourceOverride() {
    if (had_shared_instance_) {
      // Reloading the resources will discard all overrides.
      bundle_.ReloadLocaleResources(app_locale_);
    } else {
      ui::ResourceBundle::CleanupSharedInstance();
    }
  }

  void OverrideLocaleStringResource(int string_id, const std::u16string& str) {
    bundle_.OverrideLocaleStringResource(string_id, str);
  }

 private:
  // Returns the shared resource bundle. Creates one if there was none.
  static ui::ResourceBundle& GetOrCreateSharedInstance() {
    if (!ui::ResourceBundle::HasSharedInstance()) {
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          "en", nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
    }
    return ui::ResourceBundle::GetSharedInstance();
  }

  const bool had_shared_instance_;  // Was there a shared bundle before?
  ui::ResourceBundle& bundle_;      // The shared bundle.
  const std::string app_locale_;
};

const struct {
  const char* const user_visible_url;
  const char* const form_origin_url;
  PasswordTitleType bubble_type;
  const char* const expected_domain_placeholder;  // domain name
} kDomainsTestCases[] = {
    // Same domains.
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     PasswordTitleType::SAVE_PASSWORD, ""},
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     PasswordTitleType::SAVE_PASSWORD, ""},

    // Different subdomains.
    {"https://a.example.com/landing",
     "https://b.example.com/login#form?value=3",
     PasswordTitleType::SAVE_PASSWORD, ""},
    {"https://a.example.com/landing",
     "https://b.example.com/login#form?value=3",
     PasswordTitleType::SAVE_PASSWORD, ""},

    // Different domains.
    {"https://another.org", "https://example.com:/login#form?value=3",
     PasswordTitleType::SAVE_PASSWORD, "example.com"},
    {"https://another.org", "https://example.com/login#form?value=3",
     PasswordTitleType::SAVE_PASSWORD, "example.com"},

    // Different domains and password form origin url with
    // default port for the scheme.
    {"https://another.org", "https://example.com:443/login#form?value=3",
     PasswordTitleType::SAVE_PASSWORD, "example.com"},
    {"https://another.org", "http://example.com:80/login#form?value=3",
     PasswordTitleType::SAVE_PASSWORD, "example.com"},

    // Different domains and password form origin url with
    // non-default port for the scheme.
    {"https://another.org", "https://example.com:8001/login#form?value=3",
     PasswordTitleType::SAVE_PASSWORD, "example.com:8001"},
    {"https://another.org", "https://example.com:8001/login#form?value=3",
     PasswordTitleType::SAVE_PASSWORD, "example.com:8001"},

    // Update bubble, same domains.
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     PasswordTitleType::UPDATE_PASSWORD, ""},
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     PasswordTitleType::UPDATE_PASSWORD, ""},

    // Update bubble, different domains.
    {"https://another.org", "http://example.com/login#form?value=3",
     PasswordTitleType::UPDATE_PASSWORD, "example.com"},
    {"https://another.org", "http://example.com/login#form?value=3",
     PasswordTitleType::UPDATE_PASSWORD, "example.com"},

    // Same domains, federated credential.
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     PasswordTitleType::SAVE_ACCOUNT, ""},
    {"http://example.com/landing", "http://example.com/login#form?value=3",
     PasswordTitleType::SAVE_ACCOUNT, ""},

    // Different subdomains, federated credential.
    {"https://a.example.com/landing",
     "https://b.example.com/login#form?value=3",
     PasswordTitleType::SAVE_ACCOUNT, ""},
    {"https://a.example.com/landing",
     "https://b.example.com/login#form?value=3",
     PasswordTitleType::SAVE_ACCOUNT, ""}};

}  // namespace

// Test for GetSavePasswordDialogTitleText().
TEST(ManagePasswordsViewUtilTest, GetSavePasswordDialogTitleText) {
  for (size_t i = 0; i < std::size(kDomainsTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "user_visible_url = "
                                    << kDomainsTestCases[i].user_visible_url
                                    << ", form_origin_url = "
                                    << kDomainsTestCases[i].form_origin_url);

    std::u16string title = GetSavePasswordDialogTitleText(
        GURL(kDomainsTestCases[i].user_visible_url),
        url::Origin::Create(GURL(kDomainsTestCases[i].form_origin_url)),
        kDomainsTestCases[i].bubble_type);

    // Verify against expectations.
    std::u16string domain =
        base::ASCIIToUTF16(kDomainsTestCases[i].expected_domain_placeholder);
    EXPECT_TRUE(title.find(domain) != std::u16string::npos);
    if (kDomainsTestCases[i].bubble_type ==
        PasswordTitleType::UPDATE_PASSWORD) {
      EXPECT_TRUE(title.find(u"Update") != std::u16string::npos);
    } else {
      EXPECT_TRUE(title.find(u"Save") != std::u16string::npos);
    }
  }
}

// Check that empty localised strings do not cause a crash.
TEST(ManagePasswordsViewUtilTest, GetSavePasswordDialogTitleText_EmptyStrings) {
  ScopedResourceOverride resource_override;

  // Ensure that the resource bundle returns an empty string for the UI.
  resource_override.OverrideLocaleStringResource(IDS_SAVE_PASSWORD,
                                                 std::u16string());

  const GURL kExample("http://example.org");
  // The arguments passed below have this importance for the codepath:
  // * The first two URLs need to be the same, otherwise
  //   IDS_SAVE_PASSWORD_DIFFERENT_DOMAINS_TITLE will be used instead of
  //   IDS_SAVE_PASSWORD overridden above.
  // * |kBrandingEnabled| needs to be true, otherwise the code won't try to
  //   dereference placeholder offsets from the localised string, which
  //   triggers the crash in http://crbug.com/658902.
  // * SAVE_PASSWORD dialog type needs to be passed to match the
  //   IDS_SAVE_PASSWORD overridden above.
  std::u16string title =
      GetSavePasswordDialogTitleText(kExample, url::Origin::Create(kExample),
                                     PasswordTitleType::SAVE_PASSWORD);
  // Verify that the test did not pass just because
  // GetSavePasswordDialogTitleText changed the resource IDs it uses
  // (and hence did not get the overridden empty string). If the empty localised
  // string was used, the title and the range will be empty as well.
  EXPECT_THAT(title, testing::IsEmpty());
}

TEST(ManagePasswordsViewUtilTest, GetManagePasswordsDialogTitleText) {
  for (size_t i = 0; i < std::size(kDomainsTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "user_visible_url = "
                                    << kDomainsTestCases[i].user_visible_url
                                    << ", password_origin_url = "
                                    << kDomainsTestCases[i].form_origin_url);

    std::u16string title = GetManagePasswordsDialogTitleText(
        GURL(kDomainsTestCases[i].user_visible_url),
        url::Origin::Create(GURL(kDomainsTestCases[i].form_origin_url)), true);

    // Verify against expectations.
    std::u16string domain =
        base::ASCIIToUTF16(kDomainsTestCases[i].expected_domain_placeholder);
    EXPECT_TRUE(title.find(domain) != std::u16string::npos);
  }
}

TEST(ManagePasswordsViewUtilTest,
     GetConfirmationManagePasswordsDialogTitleText) {
  EXPECT_NE(std::u16string(), GetConfirmationManagePasswordsDialogTitleText());
}
