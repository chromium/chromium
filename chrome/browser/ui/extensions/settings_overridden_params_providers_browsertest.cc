// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"

#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/test_extension_dir.h"

class SettingsOverriddenParamsProvidersBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    search_test_utils::WaitForTemplateURLServiceToLoad(
        TemplateURLServiceFactory::GetForProfile(browser()->profile()));
  }

  // Installs a new extension that controls the default search engine.
  const extensions::Extension* AddExtensionControllingSearch(
      const char* path = "search_provider_override") {
    const extensions::Extension* extension =
        InstallExtensionWithPermissionsGranted(
            test_data_dir_.AppendASCII("search_provider_override"), 1);
    EXPECT_EQ(extension,
              extensions::GetExtensionOverridingSearchEngine(profile()));
    return extension;
  }

  // Installs a new extension that controls the new tab page.
  const extensions::Extension* AddExtensionControllingNewTab() {
    const extensions::Extension* extension =
        InstallExtensionWithPermissionsGranted(
            test_data_dir_.AppendASCII("api_test/override/newtab"), 1);
    EXPECT_EQ(extension,
              extensions::GetExtensionOverridingNewTabPage(profile()));
    return extension;
  }

  // Sets a new default search provider. The new search provider will be one
  // shows in the default search provider list iff
  // |new_search_shows_in_default_list| is true.  If non-null,
  // |new_search_name_out| will be populated with the new search provider's
  // name.
  void SetNewDefaultSearch(bool new_search_shows_in_default_list,
                           const TemplateURL** new_turl_out) {
    // Find a search provider that isn't Google, and set it as the default.
    TemplateURLService* const template_url_service = GetTemplateURLService();
    TemplateURLService::TemplateURLVector template_urls =
        template_url_service->GetTemplateURLs();
    auto iter = base::ranges::find_if(
        template_urls, [template_url_service, new_search_shows_in_default_list](
                           const TemplateURL* turl) {
          return !turl->HasGoogleBaseURLs(
                     template_url_service->search_terms_data()) &&
                 template_url_service->ShowInDefaultList(turl) ==
                     new_search_shows_in_default_list;
        });
    ASSERT_NE(template_urls.end(), iter);
    // iter != template_urls.end());
    template_url_service->SetUserSelectedDefaultSearchProvider(*iter);
    if (new_turl_out)
      *new_turl_out = *iter;
  }

  TemplateURLService* GetTemplateURLService() {
    return TemplateURLServiceFactory::GetForProfile(profile());
  }
};

// The chrome_settings_overrides API that allows extensions to override the
// default search provider is only available on Windows and Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// NOTE: It's very unfortunate that this has to be a browsertest. Unfortunately,
// a few bits here - the TemplateURLService in particular - don't play nicely
// with a unittest environment.
IN_PROC_BROWSER_TEST_F(SettingsOverriddenParamsProvidersBrowserTest,
                       GetExtensionControllingSearch) {
  // With no extensions installed, there should be no controlling extension.
  EXPECT_EQ(std::nullopt,
            settings_overridden_params::GetSearchOverriddenParams(profile()));

  // Install an extension, but not one that overrides the default search engine.
  // There should still be no controlling extension.
  InstallExtensionWithPermissionsGranted(
      test_data_dir_.AppendASCII("simple_with_icon"), 1);
  EXPECT_EQ(std::nullopt,
            settings_overridden_params::GetSearchOverriddenParams(profile()));

  // Finally, install an extension that overrides the default search engine.
  // It should be the controlling extension.
  const extensions::Extension* search_extension =
      AddExtensionControllingSearch();
  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetSearchOverriddenParams(profile());
  ASSERT_TRUE(params);
  EXPECT_EQ(search_extension->id(), params->controlling_extension_id);

  EXPECT_EQ(u"Change back to Google Search?", params->dialog_title);

  // Validate the body message, since it has a bit of formatting applied.
  EXPECT_EQ(
      u"The \"Search Override Extension\" extension changed search to use "
      "example.com",
      params->dialog_message);
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenParamsProvidersBrowserTest,
                       GetExtensionControllingSearch_NonGoogleSearch) {
  constexpr bool kNewSearchShowsInDefaultList = true;
  const TemplateURL* new_turl = nullptr;
  SetNewDefaultSearch(kNewSearchShowsInDefaultList, &new_turl);
  ASSERT_TRUE(new_turl);
  std::string new_search_name = base::UTF16ToUTF8(new_turl->short_name());

  const extensions::Extension* extension = AddExtensionControllingSearch();
  ASSERT_TRUE(extension);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetSearchOverriddenParams(profile());
  ASSERT_TRUE(params);
  EXPECT_EQ(base::StringPrintf("Change back to %s?", new_search_name.c_str()),
            base::UTF16ToUTF8(params->dialog_title));
}

IN_PROC_BROWSER_TEST_F(SettingsOverriddenParamsProvidersBrowserTest,
                       GetExtensionControllingSearch_NonDefaultSearch) {
  // Create and set a search provider that isn't one of the built-in default
  // options.
  TemplateURLService* const template_url_service = GetTemplateURLService();
  template_url_service->Add(
      std::make_unique<TemplateURL>(*GenerateDummyTemplateURLData("test")));

  constexpr bool kNewSearchShowsInDefaultList = false;
  SetNewDefaultSearch(kNewSearchShowsInDefaultList, nullptr);

  const extensions::Extension* extension = AddExtensionControllingSearch();
  ASSERT_TRUE(extension);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetSearchOverriddenParams(profile());
  ASSERT_TRUE(params);
  EXPECT_EQ("Did you mean to change your search provider?",
            base::UTF16ToUTF8(params->dialog_title));
}

IN_PROC_BROWSER_TEST_F(
    SettingsOverriddenParamsProvidersBrowserTest,
    GetExtensionControllingSearch_MultipleSearchProvidingExtensions) {
  const extensions::Extension* first = AddExtensionControllingSearch();
  ASSERT_TRUE(first);

  const extensions::Extension* second =
      AddExtensionControllingSearch("search_provider_override2");
  ASSERT_TRUE(second);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetSearchOverriddenParams(profile());
  ASSERT_TRUE(params);
  EXPECT_EQ(u"Did you mean to change your search provider?",
            params->dialog_title);
}

// Tests that null params are returned (indicating no dialog should be shown)
// when an extension overrides search to the same domain that was previously
// used using a prepopulated id.
IN_PROC_BROWSER_TEST_F(SettingsOverriddenParamsProvidersBrowserTest,
                       SearchOverriddenToSameSearch_PrepopulatedId) {
  constexpr bool kNewSearchShowsInDefaultList = true;
  const TemplateURL* new_turl = nullptr;
  SetNewDefaultSearch(kNewSearchShowsInDefaultList, &new_turl);
  ASSERT_TRUE(new_turl);
  // Google's ID is the lowest valid ID (1); the new engine must be greater.
  constexpr int kGooglePrepopulateId = 1;
  EXPECT_GT(new_turl->prepopulate_id(), kGooglePrepopulateId);

  constexpr char kManifestTemplate[] =
      R"({
           "name": "Search Override Extension",
           "version": "0.1",
           "manifest_version": 2,
           "chrome_settings_overrides": {
             "search_provider": {
               "search_url": "%s/?q={searchTerms}",
               "prepopulated_id": %d,
               "is_default": true
             }
           },
           "permissions": ["storage"]
         })";
  extensions::TestExtensionDir test_dir;

  GURL search_url =
      new_turl->GenerateSearchURL(GetTemplateURLService()->search_terms_data());
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, search_url.DeprecatedGetOriginAsURL().spec().c_str(),
      new_turl->prepopulate_id()));

  const extensions::Extension* extension =
      InstallExtensionWithPermissionsGranted(test_dir.UnpackedPath(), 1);
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension,
            extensions::GetExtensionOverridingSearchEngine(profile()));

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetSearchOverriddenParams(profile());
  EXPECT_FALSE(params) << "Unexpected params: " << params->dialog_title;
}

// Tests that null params are returned (indicating no dialog should be shown)
// when an extension overrides search to the same domain that was previously
// used using a custom search definition.
IN_PROC_BROWSER_TEST_F(SettingsOverriddenParamsProvidersBrowserTest,
                       SearchOverriddenToSameSearch_SameDomain) {
  constexpr bool kNewSearchShowsInDefaultList = true;
  const TemplateURL* new_turl = nullptr;
  SetNewDefaultSearch(kNewSearchShowsInDefaultList, &new_turl);
  ASSERT_TRUE(new_turl);
  // Google's ID is the lowest valid ID (1); the new engine must be greater.
  constexpr int kGooglePrepopulateId = 1;
  EXPECT_GT(new_turl->prepopulate_id(), kGooglePrepopulateId);

  constexpr char kManifestTemplate[] =
      R"({
           "name": "Search Override Extension",
           "version": "0.1",
           "manifest_version": 2,
           "chrome_settings_overrides": {
             "search_provider": {
               "search_url": "%s/?q={searchTerms}",
               "name": "New Search",
               "keyword": "word",
               "encoding": "UTF-8",
               "favicon_url": "https://example.com/favicon.ico",
               "is_default": true
             }
           },
           "permissions": ["storage"]
         })";

  extensions::TestExtensionDir test_dir;

  GURL search_url =
      new_turl->GenerateSearchURL(GetTemplateURLService()->search_terms_data());
  test_dir.WriteManifest(base::StringPrintf(
      kManifestTemplate, search_url.DeprecatedGetOriginAsURL().spec().c_str()));

  const extensions::Extension* extension =
      InstallExtensionWithPermissionsGranted(test_dir.UnpackedPath(), 1);
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension,
            extensions::GetExtensionOverridingSearchEngine(profile()));

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetSearchOverriddenParams(profile());
  EXPECT_FALSE(params) << "Unexpected params: " << params->dialog_title;
}

class LightweightSettingsOverriddenParamsProvidersBrowserTest
    : public SettingsOverriddenParamsProvidersBrowserTest {
 public:
  LightweightSettingsOverriddenParamsProvidersBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kLightweightExtensionOverrideConfirmations);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that, with the lightweight settings overrides feature enabled, the
// settings overridden dialog isn't shown for a simple override extension, but
// would be if the extension is then updated to have more capabilities.
IN_PROC_BROWSER_TEST_F(LightweightSettingsOverriddenParamsProvidersBrowserTest,
                       DialogNotShownForSimpleOverridesAndIsAfterUpdate) {
  extensions::TestExtensionDir dir_v1;
  static constexpr char kManifestV1[] =
      R"({
           "name": "Search Override",
           "version": "0.1",
           "manifest_version": 3,
           "chrome_settings_overrides": {
             "search_provider": {
               "search_url": "https://example.com/?q={searchTerms}",
               "name": "New Search",
               "keyword": "word",
               "encoding": "UTF-8",
               "favicon_url": "https://example.com/favicon.ico",
               "is_default": true
             }
           }
         })";
  dir_v1.WriteManifest(kManifestV1);
  dir_v1.WriteFile(FILE_PATH_LITERAL("page.html"), "hello world!");

  extensions::TestExtensionDir dir_v2;
  static constexpr char kManifestV2[] =
      R"({
           "name": "Search Override",
           "version": "0.2",
           "manifest_version": 3,
           "chrome_settings_overrides": {
             "search_provider": {
               "search_url": "https://example.com/?q={searchTerms}",
               "name": "New Search",
               "keyword": "word",
               "encoding": "UTF-8",
               "favicon_url": "https://example.com/favicon.ico",
               "is_default": true
             }
           },
           "permissions": ["storage"]
         })";
  dir_v2.WriteManifest(kManifestV2);
  dir_v2.WriteFile(FILE_PATH_LITERAL("page.html"), "hello world!");

  // Borrow a .pem file to have consistent IDs in the .crx files.
  base::FilePath pem_path =
      test_data_dir_.AppendASCII("permissions/update.pem");

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir scoped_temp_dir;
  EXPECT_TRUE(scoped_temp_dir.CreateUniqueTempDir());

  base::FilePath v1_crx_path = PackExtensionWithOptions(
      dir_v1.UnpackedPath(), scoped_temp_dir.GetPath().AppendASCII("v1.crx"),
      pem_path, base::FilePath());
  base::FilePath v2_crx_path = PackExtensionWithOptions(
      dir_v2.UnpackedPath(), scoped_temp_dir.GetPath().AppendASCII("v2.crx"),
      pem_path, base::FilePath());

  // Install v1 of the extension. Since this is a simple override, the dialog
  // should not display.
  const extensions::Extension* extension =
      InstallExtensionWithPermissionsGranted(v1_crx_path, 1);
  ASSERT_TRUE(extension);

  {
    std::optional<ExtensionSettingsOverriddenDialog::Params> params =
        settings_overridden_params::GetSearchOverriddenParams(profile());
    ASSERT_TRUE(params);
    ExtensionSettingsOverriddenDialog controller(std::move(*params), profile());
    EXPECT_FALSE(controller.ShouldShow());
  }

  // Update the extension to v2. Now, the dialog *should* show, since the
  // extension is no longer considered a simple override.
  extension = UpdateExtension(extension->id(), v2_crx_path, 0);
  EXPECT_TRUE(extension);

  {
    std::optional<ExtensionSettingsOverriddenDialog::Params> params =
        settings_overridden_params::GetSearchOverriddenParams(profile());
    ASSERT_TRUE(params);
    ExtensionSettingsOverriddenDialog controller(std::move(*params), profile());
    EXPECT_TRUE(controller.ShouldShow());
  }
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// Tests the dialog display when the default search engine has changed; in this
// case, we should display the generic dialog.
IN_PROC_BROWSER_TEST_F(SettingsOverriddenParamsProvidersBrowserTest,
                       DialogParamsWithNonDefaultSearch) {
  // Find a search provider that isn't Google, and set it as the default.
  TemplateURLService* const template_url_service = GetTemplateURLService();
  TemplateURLService::TemplateURLVector template_urls =
      template_url_service->GetTemplateURLs();
  auto iter = base::ranges::find_if_not(
      template_urls, [template_url_service](const TemplateURL* turl) {
        // For the test, we can be a bit lazier and just use HasGoogleBaseURLs()
        // instead of getting the full search URL.
        return turl->HasGoogleBaseURLs(
            template_url_service->search_terms_data());
      });
  ASSERT_TRUE(iter != template_urls.end());
  template_url_service->SetUserSelectedDefaultSearchProvider(*iter);

  const extensions::Extension* extension = AddExtensionControllingNewTab();

  // The dialog should be the generic version, rather than prompting to go back
  // to the default.
  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetNtpOverriddenParams(profile());
  ASSERT_TRUE(params);
  EXPECT_EQ(extension->id(), params->controlling_extension_id);
  EXPECT_EQ(u"Did you mean to change this page?", params->dialog_title);
}
