// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/url_identity.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using Type = UrlIdentity::Type;
using DefaultFormatOptions = UrlIdentity::DefaultFormatOptions;
using FormatOptions = UrlIdentity::FormatOptions;
using TypeSet = UrlIdentity::TypeSet;
using testing::Eq;
using testing::Field;

namespace {
struct TestCase {
  GURL url;
  TypeSet allowed_types;
  FormatOptions options;
  UrlIdentity expected_result;
};

constexpr base::StringPiece kTestIsolatedWebAppUrl =
    "isolated-app://"
    "berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr base::StringPiece kTestIsolatedWebAppName = "Test IWA Name";
constexpr base::StringPiece kTestExtensionId =
    "0264075e-fd33-4a20-8484-b834afb0333d";
}  // namespace

class UrlIdentityTest : public testing::Test {
 protected:
  void SetUp() override {
#if !BUILDFLAG(IS_ANDROID)
    InstallIsolatedWebApp();
    InstallExtension();
#endif  // !BUILDFLAG(IS_ANDROID)
  }

#if !BUILDFLAG(IS_ANDROID)
  void InstallIsolatedWebApp() {
    web_app::test::AwaitStartWebAppProviderAndSubsystems(&testing_profile_);
    std::string iwa_name(kTestIsolatedWebAppName);
    GURL iwa_url(kTestIsolatedWebAppUrl);
    web_app::AddDummyIsolatedAppToRegistry(&testing_profile_, iwa_url,
                                           iwa_name);
  }

  void InstallExtension() {
    std::string extension_id(kTestExtensionId);
    auto extension = extensions::ExtensionBuilder("Test Extension 1")
                         .SetID(extension_id)
                         .Build();
    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(profile());
    extension_registry->AddEnabled(extension);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  Profile* profile() { return &testing_profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

TEST_F(UrlIdentityTest, AllowlistedTypesAreAllowed) {
  std::string extension_id(kTestExtensionId);
  std::vector<TestCase> test_cases = {
    {GURL("http://example.com"),
     {Type::kDefault},
     {},
     {
         .type = Type::kDefault,
         .name = u"http://example.com",
     }},
#if !BUILDFLAG(IS_ANDROID)
    {GURL(kTestIsolatedWebAppUrl),
     {Type::kIsolatedWebApp},
     {},
     {
         .type = Type::kIsolatedWebApp,
         .name = u"Test IWA Name",
     }},
    {extensions::Extension::GetBaseURLFromExtensionId(extension_id),
     {Type::kChromeExtension},
     {},
     {
         .type = Type::kChromeExtension,
         .name = u"Test Extension 1",
     }},
#endif  // !BUILDFLAG(IS_ANDROID)
    {GURL("file:///tmp/index.html"),
     {Type::kFile},
     {},
     {
         .type = Type::kFile,
         .name = u"file:///tmp/index.html",
     }},
  };

  for (const auto& test_case : test_cases) {
    UrlIdentity result = UrlIdentity::CreateFromUrl(
        profile(), test_case.url, test_case.allowed_types, test_case.options);
    EXPECT_EQ(result.name, test_case.expected_result.name);
    EXPECT_EQ(result.type, test_case.expected_result.type);
  }
}

TEST_F(UrlIdentityTest, DefaultFormatOptionsTest) {
  std::vector<TestCase> test_cases = {
      {GURL("https://example.com"),
       {Type::kDefault},
       {},
       {
           .type = Type::kDefault,
           .name = u"https://example.com",
       }},
      {GURL("https://example.com/123"),
       {Type::kDefault},
       {},
       {
           .type = Type::kDefault,
           .name = u"https://example.com",
       }},
      {GURL("https://example.com/123"),
       {Type::kDefault},
       {.default_options = {DefaultFormatOptions::kRawSpec}},
       {
           .type = Type::kDefault,
           .name = u"https://example.com/123",
       }},
      {GURL("https://example.com"),
       {Type::kDefault},
       {.default_options = {DefaultFormatOptions::kOmitCryptographicScheme}},
       {
           .type = Type::kDefault,
           .name = u"example.com",
       }},
      {GURL("https://abc.example.com"),
       {Type::kDefault},
       {.default_options = {DefaultFormatOptions::kHostname}},
       {
           .type = Type::kDefault,
           .name = u"abc.example.com",
       }},
  };

  for (const auto& test_case : test_cases) {
    UrlIdentity result = UrlIdentity::CreateFromUrl(
        profile(), test_case.url, test_case.allowed_types, test_case.options);
    EXPECT_EQ(result.name, test_case.expected_result.name);
    EXPECT_EQ(result.type, test_case.expected_result.type);
  }
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(UrlIdentityTest, ChromeExtensionsOptionsTest) {
  std::string extension_id(kTestExtensionId);
  std::vector<TestCase> test_cases = {
      {extensions::Extension::GetBaseURLFromExtensionId(extension_id),
       {Type::kChromeExtension},
       {},
       {
           .type = Type::kChromeExtension,
           .name = u"Test Extension 1",
       }}};

  for (const auto& test_case : test_cases) {
    UrlIdentity result = UrlIdentity::CreateFromUrl(
        profile(), test_case.url, test_case.allowed_types, test_case.options);
    EXPECT_EQ(result.name, test_case.expected_result.name);
    EXPECT_EQ(result.type, test_case.expected_result.type);
  }
}

TEST_F(UrlIdentityTest, IsolatedWebAppsOptionsTest) {
  std::vector<TestCase> test_cases = {
      {GURL(kTestIsolatedWebAppUrl),
       {Type::kIsolatedWebApp},
       {},
       {
           .type = Type::kIsolatedWebApp,
           .name = u"Test IWA Name",
       }},
      {GURL("isolated-app://unknown"),
       {Type::kIsolatedWebApp},
       {},
       {
           .type = Type::kDefault,
           .name = u"isolated-app://unknown",
       }},
      {GURL("isolated-app://unknown"),
       {Type::kIsolatedWebApp},
       {.default_options = {DefaultFormatOptions::kHostname}},
       {
           .type = Type::kDefault,
           .name = u"unknown",
       }},
  };

  for (const auto& test_case : test_cases) {
    UrlIdentity result = UrlIdentity::CreateFromUrl(
        profile(), test_case.url, test_case.allowed_types, test_case.options);
    EXPECT_EQ(result.name, test_case.expected_result.name);
    EXPECT_EQ(result.type, test_case.expected_result.type);
  }
}

TEST_F(UrlIdentityTest, IsolatedWebAppFallsBackIfNoWebAppProvider) {
  TestingProfile no_provider_profile;

  UrlIdentity result = UrlIdentity::CreateFromUrl(&no_provider_profile,
                                                  GURL(kTestIsolatedWebAppUrl),
                                                  {Type::kIsolatedWebApp}, {});

  EXPECT_EQ(result.type, Type::kDefault);
  EXPECT_EQ(result.name,
            u"isolated-app://"
            u"berugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic");
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(UrlIdentityTest, FileOptionsTest) {
  std::vector<TestCase> test_cases = {
      {GURL("file:///tmp/index.html"),
       {Type::kFile},
       {},
       {
           .type = Type::kFile,
           .name = u"file:///tmp/index.html",
       }},
  };

  for (const auto& test_case : test_cases) {
    UrlIdentity result = UrlIdentity::CreateFromUrl(
        profile(), test_case.url, test_case.allowed_types, test_case.options);
    EXPECT_EQ(result.name, test_case.expected_result.name);
    EXPECT_EQ(result.type, test_case.expected_result.type);
  }
}
