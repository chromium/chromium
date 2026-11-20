// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_url_overrides.h"
#include "chrome/browser/extensions/extension_url_overrides_registrar.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"  // IWYU pragma: keep
#include "chrome/browser/resources_util.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"
#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"
#include "chrome/browser/ui/extensions/search_override_stack.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/crx_file/id_util.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/image_fetcher/core/image_fetcher_types.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/model/string_ordinal.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_helper.h"
#include "extensions/browser/extension_prefs_helper_factory.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/permissions/permissions_updater.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "skia/ext/image_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"  // IWYU pragma: keep
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

namespace {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

using ::testing::_;

constexpr char kSearchOverrideExtensionName[] = "Search Override Extension";
constexpr char16_t kSearchOverrideExtensionName16[] =
    u"Search Override Extension";
constexpr char kLongSearchOverrideExtensionName[] =
    "This extension name should be longer than our truncation threshold "
    "to test that the bubble can handle long names";

// Mock ImageFetcherService used to verify that search engine favicons are
// fetched as expected for the explicit-choice dialog without performing network
// requests in unit tests.
class MockImageFetcherService : public image_fetcher::ImageFetcherService {
 public:
  MockImageFetcherService() = default;
  ~MockImageFetcherService() override = default;

  testing::NiceMock<image_fetcher::MockImageFetcher>* mock_image_fetcher() {
    return &mock_image_fetcher_;
  }

  // image_fetcher::ImageFetcherService:
  image_fetcher::ImageFetcher* GetImageFetcher(
      image_fetcher::ImageFetcherConfig config) override {
    return &mock_image_fetcher_;
  }

 private:
  testing::NiceMock<image_fetcher::MockImageFetcher> mock_image_fetcher_;
};

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}  // namespace

class SettingsOverriddenParamsProvidersUnitTest
    : public extensions::ExtensionServiceTestBase {
 public:
  SettingsOverriddenParamsProvidersUnitTest()
      : extensions::ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::MainThreadType::IO,
                base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();

    SetUpExtensionService();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
    SetUpMockImageFetcherForProfile(profile());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

    search_test_utils::WaitForTemplateURLServiceToLoad(GetTemplateURLService());
    GetTemplateURLService()->RepairPrepopulatedSearchEngines();
  }

  void SetUpExtensionService() {
    ExtensionServiceInitParams params;
    // Use an empty preferences file so ExtensionServiceTestBase wires
    // `policy_provider()` into the profile's PrefService.
    params.prefs_content = "";

    // The NtpOverriddenDialogController relies on ExtensionUrlOverrides;
    // ensure one exists.
    params.testing_factories.emplace_back(
        extensions::ExtensionUrlOverridesRegistrar::GetFactoryInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<extensions::ExtensionUrlOverridesRegistrar>(
              context);
        }));

    // SettingsOverridesAPI relies on ExtensionPrefsHelper and
    // TemplateURLService, both of which default to null in unit tests unless
    // testing factories are registered prior to profile initialization.
    params.testing_factories.emplace_back(
        extensions::ExtensionPrefsHelperFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<extensions::ExtensionPrefsHelper>(
              extensions::ExtensionPrefs::Get(context),
              ExtensionPrefValueMapFactory::GetForBrowserContext(context));
        }));
    params.testing_factories.emplace_back(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));

    InitializeExtensionService(std::move(params));

    // Connect an ExtensionPrefStore backed by ExtensionPrefValueMap to the
    // profile's PrefService so that extension-controlled preferences set by
    // SettingsOverridesAPI are observed by DefaultSearchManager.
    profile()->GetPrefs()->UpdateExtensionPrefStore(
        base::MakeRefCounted<ExtensionPrefStore>(
            ExtensionPrefValueMapFactory::GetForBrowserContext(profile()),
            /*incognito_pref_store=*/false));
  }

  // Installs `extension` with active permissions granted, advances mock time so
  // sequential installations have strictly increasing timestamps, and returns
  // the enabled instance from the registry.
  const extensions::Extension* InstallExtension(
      scoped_refptr<const extensions::Extension> extension) {
    const extensions::ExtensionId id = extension->id();
    task_environment()->FastForwardBy(base::Seconds(1));
    extensions::PermissionsUpdater(profile()).GrantActivePermissions(
        extension.get());
    registrar()->OnExtensionInstalled(
        extension.get(), syncer::StringOrdinal::CreateInitialOrdinal(),
        extensions::kInstallFlagInstallImmediately);
    task_environment()->FastForwardBy(base::Seconds(1));
    const extensions::Extension* enabled_extension =
        registry()->enabled_extensions().GetByID(id);
    CHECK(enabled_extension);
    return enabled_extension;
  }

  std::string GenerateUniqueExtensionId(std::string_view seed) {
    return crx_file::id_util::GenerateId(
        base::StrCat({seed, "_", base::NumberToString(next_extension_id_++)}));
  }

  // Adds a new extension that overrides the NTP.
  const extensions::Extension* AddExtensionControllingNewTab(
      const std::string& name = "ntp override") {
    base::DictValue chrome_url_overrides =
        base::DictValue().Set("newtab", "newtab.html");
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(name)
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .SetID(GenerateUniqueExtensionId(name))
            .SetManifestKey("chrome_url_overrides",
                            std::move(chrome_url_overrides))
            .Build();

    const extensions::Extension* installed_extension =
        InstallExtension(std::move(extension));
    EXPECT_EQ(installed_extension,
              ExtensionUrlOverrides::GetExtensionControllingURL(
                  chrome::ChromeUINewTabURLAsGURL(), profile()));

    return installed_extension;
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // Installs the mock image fetcher on the given profile.
  void SetUpMockImageFetcherForProfile(Profile* target_profile) {
    CHECK(target_profile);
    auto* image_fetcher_service = static_cast<MockImageFetcherService*>(
        ImageFetcherServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            target_profile->GetProfileKey(),
            base::BindRepeating(
                [](SimpleFactoryKey* key) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<MockImageFetcherService>();
                })));
    CHECK(image_fetcher_service);
    EXPECT_CALL(*image_fetcher_service->mock_image_fetcher(),
                FetchImageAndData_(_, _, _, _))
        .WillRepeatedly([](const GURL&,
                           image_fetcher::ImageDataFetcherCallback*,
                           image_fetcher::ImageFetcherCallback* callback,
                           image_fetcher::ImageFetcherParams) {
          std::move(*callback).Run(gfx::Image(),
                                   image_fetcher::RequestMetadata());
        });
  }

  // Makes asynchronous parameter collection synchronous for testing, by waiting
  // in a run loop for icon resource fetching to complete.
  std::optional<ExtensionSettingsOverriddenDialog::Params>
  GetSearchOverriddenParamsSync() {
    return GetSearchOverriddenParamsSync(profile());
  }

  std::optional<ExtensionSettingsOverriddenDialog::Params>
  GetSearchOverriddenParamsSync(Profile* target_profile) {
    std::optional<ExtensionSettingsOverriddenDialog::Params> result;
    base::RunLoop run_loop;
    settings_overridden_params::GetSearchOverriddenParamsThenRun(
        target_profile,
        base::BindLambdaForTesting(
            [&](std::unique_ptr<ExtensionSettingsOverriddenDialog::Params>
                    params) {
              if (params) {
                result.emplace(std::move(*params));
              }
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  // Installs a new extension that controls the default search engine. When
  // `is_simple_override` is false, includes the "storage" permission so the
  // extension is not classified as a simple override.
  const extensions::Extension* AddExtensionControllingSearch(
      const std::string& name = kSearchOverrideExtensionName,
      const std::string& keyword = "word",
      const std::string& search_url =
          "https://www.example.com/?q={searchTerms}",
      bool is_simple_override = false,
      std::optional<int> prepopulated_id = std::nullopt,
      std::optional<GURL> favicon_url = std::nullopt,
      std::optional<std::string> explicit_id = std::nullopt,
      const std::string& version = "0.1") {
    auto search_provider =
        base::DictValue().Set("search_url", search_url).Set("is_default", true);
    if (prepopulated_id) {
      search_provider.Set("prepopulated_id", *prepopulated_id);
    } else {
      search_provider.Set("name", name);
      search_provider.Set("keyword", keyword);
      search_provider.Set("encoding", "UTF-8");
    }
    if (favicon_url) {
      search_provider.Set("favicon_url", favicon_url->spec());
    } else if (!prepopulated_id) {
      search_provider.Set("favicon_url", "https://example.com/favicon.ico");
    }

    extensions::ExtensionBuilder builder(name);
    builder.SetLocation(extensions::mojom::ManifestLocation::kInternal)
        .SetID(explicit_id.value_or(GenerateUniqueExtensionId(name)))
        .SetVersion(version)
        .SetManifestKey("chrome_settings_overrides",
                        base::DictValue().Set("search_provider",
                                              std::move(search_provider)));
    if (!is_simple_override) {
      builder.AddAPIPermission("storage");
    }

    const extensions::Extension* extension = InstallExtension(builder.Build());
    EXPECT_EQ(extension,
              extensions::GetExtensionOverridingSearchEngine(profile()));
    return extension;
  }

  // Installs a simple search override extension (no permissions) searching
  // `search_host` under `keyword`.
  const extensions::Extension* AddSimpleSearchOverrideExtension(
      const std::string& name,
      const std::string& keyword,
      const std::string& search_host) {
    return AddExtensionControllingSearch(
        name, keyword,
        base::StringPrintf("https://%s/?q={searchTerms}", search_host.c_str()),
        /*is_simple_override=*/true, /*prepopulated_id=*/std::nullopt,
        GURL(
            base::StringPrintf("https://%s/favicon.ico", search_host.c_str())));
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  // Sets a new default search provider. The new search provider will be one
  // that shows in the default search provider list iff
  // `new_search_shows_in_default_list` is true. If non-null, `new_turl_out`
  // will be populated with the new search provider.
  void SetNewDefaultSearch(bool new_search_shows_in_default_list,
                           const TemplateURL** new_turl_out) {
    TemplateURLService* const template_url_service = GetTemplateURLService();
    TemplateURLService::TemplateURLVector template_urls =
        template_url_service->GetTemplateURLs();
    auto iter = std::ranges::find_if(
        template_urls, [template_url_service, new_search_shows_in_default_list](
                           const TemplateURL* turl) {
          return template_url_service->CanMakeDefault(turl) &&
                 !turl->HasGoogleBaseURLs(
                     template_url_service->search_terms_data()) &&
                 template_url_service->ShowInDefaultList(turl) ==
                     new_search_shows_in_default_list;
        });
    ASSERT_NE(template_urls.end(), iter);
    template_url_service->SetUserSelectedDefaultSearchProvider(*iter);
    if (new_turl_out) {
      *new_turl_out = *iter;
    }
  }

  TemplateURLService* GetTemplateURLService() {
    return TemplateURLServiceFactory::GetForProfile(profile());
  }

 private:
  size_t next_extension_id_ = 0;
};

TEST_F(SettingsOverriddenParamsProvidersUnitTest,
       GetExtensionControllingNewTab) {
  // With no extensions installed, there should be no controlling extension.
  EXPECT_EQ(std::nullopt,
            settings_overridden_params::GetNtpOverriddenParams(profile()));

  // Install an extension, but not one that overrides the NTP. There should
  // still be no controlling extension.
  scoped_refptr<const extensions::Extension> regular_extension =
      extensions::ExtensionBuilder("regular").Build();
  InstallExtension(regular_extension);
  EXPECT_EQ(std::nullopt,
            settings_overridden_params::GetNtpOverriddenParams(profile()));

  // Finally, install an extension that overrides the NTP. It should be the
  // controlling extension.
  const extensions::Extension* ntp_extension = AddExtensionControllingNewTab();
  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetNtpOverriddenParams(profile());
  ASSERT_TRUE(params);
  EXPECT_EQ(ntp_extension->id(), params->controlling_extension_id);

  // In this case, disabling the extension would go back to the default NTP, so
  // a specific message should show.
  EXPECT_EQ("Change back to Google?",
            base::UTF16ToUTF8(params->content.dialog_title));
}

// Tests that long extension names are truncated in the dialog message.
TEST_F(SettingsOverriddenParamsProvidersUnitTest,
       LongNameExtensionOverridingNewTab) {
  // With no extensions installed, there should be no controlling extension.
  EXPECT_EQ(std::nullopt,
            settings_overridden_params::GetNtpOverriddenParams(profile()));

  // Install an extension which overrides the NTP and has a long name.
  const std::u16string extension_name =
      u"This extension name should be longer than our truncation threshold "
      "to test that the bubble can handle long names";
  const std::u16string truncated_name =
      extensions::ui_util::GetFixupExtensionNameForUIDisplay(extension_name);
  ASSERT_LT(truncated_name.size(), extension_name.size());

  const extensions::Extension* ntp_extension =
      AddExtensionControllingNewTab(base::UTF16ToUTF8(extension_name));
  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetNtpOverriddenParams(profile());
  ASSERT_TRUE(params);
  ASSERT_EQ(ntp_extension->id(), params->controlling_extension_id);

  // The dialog message should contain the truncated name.
  EXPECT_TRUE(params->content.message.contains(truncated_name));
  EXPECT_FALSE(params->content.message.contains(extension_name));
}

TEST_F(SettingsOverriddenParamsProvidersUnitTest,
       DialogStringsWhenMultipleNtpOverrides_MultipleExtensions) {
  const extensions::Extension* extension1 =
      AddExtensionControllingNewTab("uno");
  const extensions::Extension* extension2 =
      AddExtensionControllingNewTab("dos");
  EXPECT_NE(extension1->id(), extension2->id());

  // When there are multiple extensions that could override the NTP, we should
  // show a generic dialog (rather than prompting to go back to the default
  // NTP), because the other extension would just take over.
  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      settings_overridden_params::GetNtpOverriddenParams(profile());
  ASSERT_TRUE(params);
  EXPECT_EQ(extension2->id(), params->controlling_extension_id);
  EXPECT_EQ("Did you mean to change this page?",
            base::UTF16ToUTF8(params->content.dialog_title));
}

// Tests the dialog display when the default search engine has changed; in this
// case, we should display the generic dialog.
TEST_F(SettingsOverriddenParamsProvidersUnitTest,
       DialogParamsWithNonDefaultSearch) {
  // Find a search provider that isn't Google, and set it as the default.
  TemplateURLService* const template_url_service = GetTemplateURLService();
  TemplateURLService::TemplateURLVector template_urls =
      template_url_service->GetTemplateURLs();
  auto iter = std::ranges::find_if_not(
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
  EXPECT_EQ(u"Did you mean to change this page?", params->content.dialog_title);
}

// The chrome_settings_overrides API that allows extensions to override the
// default search provider is only available on Windows and Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

class SearchOverriddenParamsProvidersUnitTest
    : public SettingsOverriddenParamsProvidersUnitTest,
      public testing::WithParamInterface<bool> {
 protected:
  SearchOverriddenParamsProvidersUnitTest() {
    feature_list_.InitWithFeatureState(
        extensions_features::kSearchEngineExplicitChoiceDialog, GetParam());
  }

  bool IsExplicitChoiceDialog() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(SearchOverriddenParamsProvidersUnitTest, GetExtensionControllingSearch) {
  // With no extensions installed, there should be no controlling extension.
  EXPECT_EQ(std::nullopt, GetSearchOverriddenParamsSync());

  // Install an extension, but not one that overrides the default search engine.
  // There should still be no controlling extension.
  InstallExtension(extensions::ExtensionBuilder("regular").Build());
  EXPECT_EQ(std::nullopt, GetSearchOverriddenParamsSync());

  // Finally, install an extension that overrides the default search engine.
  // It should be the controlling extension.
  const extensions::Extension* search_extension =
      AddExtensionControllingSearch();
  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);
  EXPECT_EQ(search_extension->id(), params->controlling_extension_id);
  if (IsExplicitChoiceDialog()) {
    EXPECT_TRUE(params->content.dialog_title.contains(u"Confirm"));
    EXPECT_TRUE(params->content.message.contains(u"Search Override Extension"));

    ASSERT_TRUE(params->content.previous_setting);
    EXPECT_EQ(u"Google", params->content.previous_setting->text);
    EXPECT_EQ(u"Your previous choice",
              params->content.previous_setting->description);

    ASSERT_TRUE(params->content.new_setting);
    EXPECT_EQ(u"example.com", params->content.new_setting->text);
    EXPECT_EQ(u"Changed to this by extension",
              params->content.new_setting->description);
  } else {
    EXPECT_EQ(u"Change back to Google Search?", params->content.dialog_title);

    // Validate the body message, since it has a bit of formatting applied.
    EXPECT_EQ(
        u"The \"Search Override Extension\" extension changed search to use "
        "example.com",
        params->content.message);
  }

  // Verify the reported extension stack: this is the only extension overriding
  // search.
  ASSERT_TRUE(params->search_override_stack);
  EXPECT_EQ(extensions::SearchOverrideStackState::kSingleOverride,
            params->search_override_stack->state);
  EXPECT_EQ(0, params->search_override_stack->unacknowledged_chain_length);
}

TEST_P(SearchOverriddenParamsProvidersUnitTest,
       LongNameExtensionControllingSearch) {
  // With no extensions installed, there should be no controlling extension.
  ASSERT_EQ(std::nullopt, GetSearchOverriddenParamsSync());

  // Install an extension which overrides the default search engine and has a
  // long name.
  const extensions::Extension* search_extension =
      AddExtensionControllingSearch(kLongSearchOverrideExtensionName);
  std::optional<const ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);
  EXPECT_EQ(search_extension->id(), params->controlling_extension_id);

  const std::u16string extension_name =
      base::UTF8ToUTF16(search_extension->name());
  const std::u16string truncated_name =
      extensions::ui_util::GetFixupExtensionNameForUIDisplay(extension_name);
  ASSERT_LT(truncated_name.size(), extension_name.size());

  EXPECT_TRUE(params->content.message.contains(truncated_name));
  EXPECT_FALSE(params->content.message.contains(extension_name));
}

TEST_P(SearchOverriddenParamsProvidersUnitTest,
       GetExtensionControllingSearch_NonGoogleSearch) {
  constexpr bool kNewSearchShowsInDefaultList = true;
  const TemplateURL* new_turl = nullptr;
  SetNewDefaultSearch(kNewSearchShowsInDefaultList, &new_turl);
  ASSERT_TRUE(new_turl);
  std::string new_search_name = base::UTF16ToUTF8(new_turl->short_name());

  const extensions::Extension* extension = AddExtensionControllingSearch();
  ASSERT_TRUE(extension);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);
  if (IsExplicitChoiceDialog()) {
    ASSERT_TRUE(params->content.previous_setting);
    EXPECT_EQ(base::UTF8ToUTF16(new_search_name),
              params->content.previous_setting->text);
  } else {
    EXPECT_EQ(base::StringPrintf("Change back to %s?", new_search_name.c_str()),
              base::UTF16ToUTF8(params->content.dialog_title));
  }
}

TEST_P(SearchOverriddenParamsProvidersUnitTest,
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
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);
  if (IsExplicitChoiceDialog()) {
    ASSERT_TRUE(params->content.previous_setting);
    EXPECT_EQ(u"testname", params->content.previous_setting->text);
  } else {
    EXPECT_EQ("Did you mean to change your search provider?",
              base::UTF16ToUTF8(params->content.dialog_title));
  }
}

TEST_P(SearchOverriddenParamsProvidersUnitTest,
       GetExtensionControllingSearch_MultipleSearchProvidingExtensions) {
  const extensions::Extension* first_extension =
      AddExtensionControllingSearch();
  ASSERT_TRUE(first_extension);

  const extensions::Extension* second_extension =
      AddSimpleSearchOverrideExtension("Simple Search Override", "word",
                                       "example.com");
  ASSERT_TRUE(second_extension);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);
  EXPECT_EQ(second_extension->id(), params->controlling_extension_id);
  if (IsExplicitChoiceDialog()) {
    ASSERT_TRUE(params->content.previous_setting);
    EXPECT_EQ(u"example.com", params->content.previous_setting->text);
    EXPECT_TRUE(params->content.previous_setting->image.IsVectorIcon());
    ASSERT_TRUE(params->content.new_setting);
    EXPECT_EQ(u"example.com", params->content.new_setting->text);
  } else {
    EXPECT_EQ(u"Did you mean to change your search provider?",
              params->content.dialog_title);
  }

  // Verify the reported extension stack: the previous choice is the first
  // extension, which the user never acknowledged, and the original default is
  // buried beneath it.
  ASSERT_TRUE(params->search_override_stack);
  EXPECT_EQ(extensions::SearchOverrideStackState::
                kUnacknowledgedChainOverNonExtensionDefault,
            params->search_override_stack->state);
  EXPECT_EQ(1, params->search_override_stack->unacknowledged_chain_length);
}

// Tests the stack of overriding extensions when the user acknowledged the
// extension beneath the controlling one.
TEST_P(SearchOverriddenParamsProvidersUnitTest,
       GetExtensionControllingSearch_AcknowledgedExtensionBeneath) {
  const extensions::Extension* first_extension =
      AddExtensionControllingSearch();
  ASSERT_TRUE(first_extension);
  extensions::ExtensionPrefs::Get(profile())->UpdateExtensionPref(
      first_extension->id(),
      ControlledHomeDialogController::kAcknowledgedPreference,
      base::Value(true));

  const extensions::Extension* second_extension =
      AddSimpleSearchOverrideExtension("Another Search Override", "other",
                                       "other.example");
  ASSERT_TRUE(second_extension);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);
  EXPECT_EQ(second_extension->id(), params->controlling_extension_id);

  // Verify the reported extension stack: reverting stops at the acknowledged
  // extension beneath the controlling one.
  ASSERT_TRUE(params->search_override_stack);
  EXPECT_EQ(extensions::SearchOverrideStackState::kAcknowledgedExtensionNext,
            params->search_override_stack->state);
  EXPECT_EQ(0, params->search_override_stack->unacknowledged_chain_length);
}

// Tests that null params are returned (indicating no dialog should be shown)
// when an extension overrides search to the same domain that was previously
// used using a prepopulated id.
TEST_P(SearchOverriddenParamsProvidersUnitTest,
       SearchOverriddenToSameSearch_PrepopulatedId) {
  constexpr bool kNewSearchShowsInDefaultList = true;
  const TemplateURL* new_turl = nullptr;
  SetNewDefaultSearch(kNewSearchShowsInDefaultList, &new_turl);
  ASSERT_TRUE(new_turl);
  // Google's ID is the lowest valid ID (1); the new engine must be greater.
  constexpr int kGooglePrepopulateId = 1;
  EXPECT_GT(new_turl->prepopulate_id(), kGooglePrepopulateId);

  GURL search_url =
      new_turl->GenerateSearchURL(GetTemplateURLService()->search_terms_data());
  const extensions::Extension* extension = AddExtensionControllingSearch(
      kSearchOverrideExtensionName, /*keyword=*/"word",
      base::StringPrintf("%s/?q={searchTerms}",
                         search_url.DeprecatedGetOriginAsURL().spec().c_str()),
      /*is_simple_override=*/false, new_turl->prepopulate_id());
  ASSERT_TRUE(extension);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  EXPECT_FALSE(params) << "Unexpected params: " << params->content.dialog_title;
}

// Tests that null params are returned (indicating no dialog should be shown)
// when an extension overrides search to the same domain that was previously
// used using a custom search definition.
TEST_P(SearchOverriddenParamsProvidersUnitTest,
       SearchOverriddenToSameSearch_SameDomain) {
  constexpr bool kNewSearchShowsInDefaultList = true;
  const TemplateURL* new_turl = nullptr;
  SetNewDefaultSearch(kNewSearchShowsInDefaultList, &new_turl);
  ASSERT_TRUE(new_turl);
  // Google's ID is the lowest valid ID (1); the new engine must be greater.
  constexpr int kGooglePrepopulateId = 1;
  EXPECT_GT(new_turl->prepopulate_id(), kGooglePrepopulateId);

  GURL search_url =
      new_turl->GenerateSearchURL(GetTemplateURLService()->search_terms_data());
  const extensions::Extension* extension = AddExtensionControllingSearch(
      kSearchOverrideExtensionName, /*keyword=*/"word",
      base::StringPrintf("%s/?q={searchTerms}",
                         search_url.DeprecatedGetOriginAsURL().spec().c_str()),
      /*is_simple_override=*/false);
  ASSERT_TRUE(extension);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  EXPECT_FALSE(params) << "Unexpected params: " << params->content.dialog_title;
}

// Tests that the settings overridden dialog isn't shown for a simple override
// extension, but would be if the extension is then updated to have more
// capabilities.
TEST_P(SearchOverriddenParamsProvidersUnitTest,
       DialogNotShownForSimpleOverridesAndIsAfterUpdate) {
  // Install v1 of the extension as a simple override (no permissions). The
  // dialog should not display.
  const extensions::Extension* extension = AddExtensionControllingSearch(
      "Search Override", "word", "https://example.com/?q={searchTerms}",
      /*is_simple_override=*/true, /*prepopulated_id=*/std::nullopt,
      /*favicon_url=*/std::nullopt, /*explicit_id=*/std::nullopt,
      /*version=*/"0.1");
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();

  {
    std::optional<ExtensionSettingsOverriddenDialog::Params> params =
        GetSearchOverriddenParamsSync();
    ASSERT_TRUE(params);
    ExtensionSettingsOverriddenDialog controller(std::move(*params),
                                                 *profile());
    EXPECT_FALSE(controller.ShouldShow());
  }

  // Update the extension to v2 by adding the "storage" permission. Now, the
  // dialog *should* show, since the extension is no longer considered a simple
  // override.
  extension = AddExtensionControllingSearch(
      "Search Override", "word", "https://example.com/?q={searchTerms}",
      /*is_simple_override=*/false, /*prepopulated_id=*/std::nullopt,
      /*favicon_url=*/std::nullopt, extension_id,
      /*version=*/"0.2");
  EXPECT_TRUE(extension);

  {
    std::optional<ExtensionSettingsOverriddenDialog::Params> params =
        GetSearchOverriddenParamsSync();
    ASSERT_TRUE(params);
    ExtensionSettingsOverriddenDialog controller(std::move(*params),
                                                 *profile());
    EXPECT_TRUE(controller.ShouldShow());
  }
}

// Tests that null params are returned (indicating no dialog should be shown)
// when an extension overrides search to the same domain that was previously
// set by another extension.
TEST_P(SearchOverriddenParamsProvidersUnitTest,
       SearchOverriddenToSameSearch_SameDomainExistingExtensionOverride) {
  // With no extensions installed, there should be no controlling extension.
  EXPECT_EQ(std::nullopt, GetSearchOverriddenParamsSync());

  // Install a simple search override extension, that shouldn't trigger the
  // prompt.
  const extensions::Extension* extension_1 = AddSimpleSearchOverrideExtension(
      "Simple Search Override", "word", "example.com");
  ASSERT_TRUE(extension_1);
  {
    std::optional<ExtensionSettingsOverriddenDialog::Params> params =
        GetSearchOverriddenParamsSync();
    ASSERT_TRUE(params);
    ExtensionSettingsOverriddenDialog controller(std::move(*params),
                                                 *profile());
    EXPECT_FALSE(controller.ShouldShow());
  }

  // Install a second search override extension, that shouldn't trigger prompt
  // since it sets the same search domain as the previous extension.
  const extensions::Extension* extension_2 = AddExtensionControllingSearch(
      "Second Search Override", "word", "https://example.com/?q={searchTerms}",
      /*is_simple_override=*/false);
  ASSERT_TRUE(extension_2);
  {
    std::optional<ExtensionSettingsOverriddenDialog::Params> params =
        GetSearchOverriddenParamsSync();
    EXPECT_FALSE(params) << "Unexpected params: "
                         << params->content.dialog_title;
  }

  // Verify the reported extension stack: no dialog is shown here, but the
  // per-profile sample still sees the stack of overriding extensions.
  base::HistogramTester histogram_tester;
  extensions::RecordDseExtensionStackStateOnce(*profile());
  histogram_tester.ExpectUniqueSample(
      "Extensions.SettingsOverridden.DseExtensionStackState",
      extensions::SearchOverrideStackState::
          kUnacknowledgedChainOverNonExtensionDefault,
      1);
  // Once per session.
  extensions::RecordDseExtensionStackStateOnce(*profile());
  histogram_tester.ExpectTotalCount(
      "Extensions.SettingsOverridden.DseExtensionStackState", 1);
}

// Param true means Explicit Choice dialog.
INSTANTIATE_TEST_SUITE_P(All,
                         SearchOverriddenParamsProvidersUnitTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "ExplicitChoice" : "KeepOrBack";
                         });

class SearchOverriddenParamsProvidersOffTheRecordUnitTest
    : public SettingsOverriddenParamsProvidersUnitTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  SearchOverriddenParamsProvidersOffTheRecordUnitTest() {
    feature_list_.InitWithFeatureStates(
        {{extensions_features::kSearchEngineExplicitChoiceDialog,
          IsExplicitChoiceDialog()},
         {enterprise_isolated_mode::kEnableEnterpriseIsolatedMode,
          IsIsolatedMode()}});
  }

  void SetUp() override {
    SettingsOverriddenParamsProvidersUnitTest::SetUp();
    if (IsIsolatedMode()) {
      profile()->GetPrefs()->SetInteger(
          enterprise_isolated_mode::kEnterpriseIsolatedModeSettings,
          static_cast<int>(
              enterprise_isolated_mode::IsolatedModeSetting::kEnabled));
    }
  }

  bool IsExplicitChoiceDialog() const { return std::get<0>(GetParam()); }
  bool IsIsolatedMode() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(SearchOverriddenParamsProvidersOffTheRecordUnitTest,
       GetExtensionControllingSearch_MultipleSearchProvidingExtensions) {
  // Installed first (older), so it becomes the backup ("secondary") extension
  // once `primary_extension` is installed after it.
  const extensions::Extension* secondary_extension =
      AddExtensionControllingSearch();
  ASSERT_TRUE(secondary_extension);

  // Installed second (newer), so it becomes the active ("primary") extension
  // controlling search.
  const extensions::Extension* primary_extension =
      AddSimpleSearchOverrideExtension("Another Search Override", "other",
                                       "other.example");
  ASSERT_TRUE(primary_extension);

  Profile* off_the_record_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(off_the_record_profile);
  EXPECT_EQ(IsIsolatedMode(),
            off_the_record_profile->IsEnterpriseIsolatedModeProfile());
  EXPECT_EQ(!IsIsolatedMode(), off_the_record_profile->IsIncognitoProfile());

  // Even though neither extension is enabled in Incognito/OTR, search
  // overrides apply from the regular profile (TemplateURLService is redirected
  // to the original profile in OTR profiles), so `secondary_extension` is
  // still used as the backup search provider in the OTR profile.
  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync(off_the_record_profile);
  ASSERT_TRUE(params);
  EXPECT_EQ(primary_extension->id(), params->controlling_extension_id);
  if (IsExplicitChoiceDialog()) {
    ASSERT_TRUE(params->content.previous_setting);
    EXPECT_EQ(u"example.com", params->content.previous_setting->text);
    EXPECT_TRUE(params->content.previous_setting->image.IsVectorIcon());
    ASSERT_TRUE(params->content.new_setting);
    EXPECT_EQ(u"other.example", params->content.new_setting->text);
  } else {
    EXPECT_EQ(u"Did you mean to change your search provider?",
              params->content.dialog_title);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SearchOverriddenParamsProvidersOffTheRecordUnitTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "ExplicitChoice" : "KeepOrBack", "_",
           std::get<1>(info.param) ? "IsolatedMode" : "Incognito"});
    });

// Tests for when an extension makes itself the default search engine, but
// enterprise policy also configures the default search engine. Mandatory policy
// takes precedence, so the extension isn't actually controlling search.
class SearchOverriddenParamsProvidersPolicyUnitTest
    : public SearchOverriddenParamsProvidersUnitTest {
 protected:
  // Sets the mandatory DefaultSearchProviderEnabled policy to `enabled`, along
  // with a search URL if `enabled` is true.
  void SetDefaultSearchProviderPolicy(bool enabled) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kDefaultSearchProviderEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(enabled), nullptr);
    if (enabled) {
      policies.Set(policy::key::kDefaultSearchProviderSearchURL,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value("https://policy.example/?q={searchTerms}"),
                   nullptr);
    }
    policy_provider()->UpdateChromePolicy(policies);
  }
};

// Tests that null params are returned (indicating no dialog should be shown)
// when the default search engine is set by policy.
TEST_P(SearchOverriddenParamsProvidersPolicyUnitTest,
       DefaultSearchSetByPolicy) {
  SetDefaultSearchProviderPolicy(/*enabled=*/true);

  // The extension is still reported as overriding search...
  const extensions::Extension* extension = AddExtensionControllingSearch();
  ASSERT_TRUE(extension);

  // ...but the default search engine comes from policy.
  TemplateURLService* const template_url_service = GetTemplateURLService();
  EXPECT_FALSE(template_url_service->IsExtensionControlledDefaultSearch());
  const TemplateURL* default_search =
      template_url_service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_TRUE(default_search->CreatedByDefaultSearchProviderPolicy());

  EXPECT_EQ(std::nullopt, GetSearchOverriddenParamsSync());
}

// Tests that null params are returned (indicating no dialog should be shown)
// when the default search engine is disabled by policy.
TEST_P(SearchOverriddenParamsProvidersPolicyUnitTest,
       DefaultSearchDisabledByPolicy) {
  SetDefaultSearchProviderPolicy(/*enabled=*/false);

  // The extension is still reported as overriding search...
  const extensions::Extension* extension = AddExtensionControllingSearch();
  ASSERT_TRUE(extension);

  // ...but there is no default search engine.
  TemplateURLService* const template_url_service = GetTemplateURLService();
  EXPECT_FALSE(template_url_service->IsExtensionControlledDefaultSearch());
  EXPECT_FALSE(template_url_service->GetDefaultSearchProvider());

  EXPECT_EQ(std::nullopt, GetSearchOverriddenParamsSync());
}

// Param true means Explicit Choice dialog.
INSTANTIATE_TEST_SUITE_P(All,
                         SearchOverriddenParamsProvidersPolicyUnitTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "ExplicitChoice" : "KeepOrBack";
                         });

// Compares the images, ensuring that varying types are compared appropriately.
bool IconsEqual(const ui::ImageModel& a, const ui::ImageModel& b) {
  if (a.IsImage() && b.IsImage()) {
    return gfx::test::AreImagesEqual(a.GetImage(), b.GetImage());
  }
  if (a.IsVectorIcon() && b.IsVectorIcon()) {
    return a.GetVectorIcon() == b.GetVectorIcon();
  }
  return false;
}

// Test class specific to the explicit-choice dialog variation.
class ExtensionControllingSearchExplicitChoiceParamsUnitTest
    : public SettingsOverriddenParamsProvidersUnitTest {
 public:
  ExtensionControllingSearchExplicitChoiceParamsUnitTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kSearchEngineExplicitChoiceDialog);
  }

  // Creates a distinct test icon on each call so tests can distinguish between
  // multiple fetched icons.
  gfx::Image CreateTestIcon() {
    return gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kGlobeIcon, extension_misc::EXTENSION_ICON_SMALLISH,
        gfx::kPlaceholderColor + next_icon_offset_++));
  }

  void ExpectNoIconFetch(const GURL& favicon_url) {
    CHECK(!favicon_url.is_empty());
    auto* service = static_cast<MockImageFetcherService*>(
        ImageFetcherServiceFactory::GetForKey(profile()->GetProfileKey()));
    ASSERT_TRUE(service);
    EXPECT_CALL(*service->mock_image_fetcher(),
                FetchImageAndData_(favicon_url, _, _, _))
        .Times(0);
  }

  // Configures mock icon fetch expectations for the given search engine and
  // returns the expected icon that should appear in the dialog setting.
  ui::ImageModel SetUpExpectedEngineIcon(const GURL& favicon_url) {
    CHECK(!favicon_url.is_empty());
    gfx::Image expected_icon = CreateTestIcon();
    auto* service = static_cast<MockImageFetcherService*>(
        ImageFetcherServiceFactory::GetForKey(profile()->GetProfileKey()));
    CHECK(service);
    EXPECT_CALL(*service->mock_image_fetcher(),
                FetchImageAndData_(favicon_url, _, _, _))
        .WillOnce([expected_icon](const GURL&,
                                  image_fetcher::ImageDataFetcherCallback*,
                                  image_fetcher::ImageFetcherCallback* callback,
                                  image_fetcher::ImageFetcherParams) {
          std::move(*callback).Run(expected_icon,
                                   image_fetcher::RequestMetadata());
        });
    return ui::ImageModel::FromImage(expected_icon);
  }

  ui::ImageModel SetUpExpectedEngineIcon(const TemplateURL& template_url) {
    if (std::optional<ui::ImageModel> builtin_icon =
            GetExpectedBuiltinIcon(template_url)) {
      ExpectNoIconFetch(template_url.favicon_url());
      return *builtin_icon;
    }
    return SetUpExpectedEngineIcon(template_url.favicon_url());
  }

  ui::ImageModel SetUpExpectedEngineIcon(
      const TemplateURLPrepopulateData::PrepopulatedEngine& engine) {
    std::unique_ptr<TemplateURLData> data =
        TemplateURLDataFromPrepopulatedEngine(engine);
    return SetUpExpectedEngineIcon(TemplateURL(*data));
  }

  // Returns the expected fallback globe icon used when no search engine
  // favicon is available.
  ui::ImageModel GetExpectedFallbackIcon() {
    return ui::ImageModel::FromVectorIcon(
        features::IsRoundedIconsEnabled() ? vector_icons::kGlobeIcon
                                          : vector_icons::kGlobeOldIcon,
        ui::kColorIcon, extension_misc::EXTENSION_ICON_SMALLISH);
  }

  // Loads and resizes the bundled icon resource for the given search engine, if
  // one exists. Note that this intentionally mirrors the production resource
  // lookup: mocking the resource bundle would still require duplicating the
  // resource ID resolution here while adding significant mock setup and global
  // state management, so reading the real bundled resource directly is simpler.
  ui::ImageModel LoadExpectedBuiltinIcon(const std::string& image_resource_id) {
    const int resource_id =
        ResourcesUtil::GetThemeResourceId(image_resource_id);
    CHECK_NE(resource_id, -1);
    const gfx::ImageSkia* image_skia =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
    CHECK(image_skia && !image_skia->isNull());
    gfx::Size icon_size(extension_misc::EXTENSION_ICON_SMALLISH,
                        extension_misc::EXTENSION_ICON_SMALLISH);
    return ui::ImageModel::FromImageSkia(
        gfx::ImageSkiaOperations::CreateResizedImage(
            *image_skia, skia::ImageOperations::RESIZE_BEST, icon_size));
  }

  std::optional<ui::ImageModel> GetExpectedBuiltinIcon(
      const TemplateURL& template_url) {
    if (!template_url.GetBaseBuiltinResourceId().has_value()) {
      return std::nullopt;
    }
    return LoadExpectedBuiltinIcon(template_url.GetBuiltinImageResourceId());
  }

  std::optional<ui::ImageModel> GetExpectedBuiltinIcon(
      const TemplateURLPrepopulateData::PrepopulatedEngine& engine) {
    std::optional<std::string> resource_id =
        TemplateURL::GetBuiltinImageResourceId(engine);
    if (!resource_id.has_value()) {
      return std::nullopt;
    }
    return LoadExpectedBuiltinIcon(*resource_id);
  }

  // Returns the first non-Google prepopulated search engine from the
  // TemplateURLService default list.
  const TemplateURL* FindPrepopulatedTemplateURL() {
    TemplateURLService* const template_url_service = GetTemplateURLService();
    TemplateURLService::TemplateURLVector template_urls =
        template_url_service->GetTemplateURLs();
    auto iter = std::ranges::find_if(
        template_urls, [template_url_service](const TemplateURL* turl) {
          return !turl->HasGoogleBaseURLs(
                     template_url_service->search_terms_data()) &&
                 template_url_service->ShowInDefaultList(turl) &&
                 turl->prepopulate_id() > 0;
        });
    CHECK(iter != template_urls.end());
    return *iter;
  }

  // Creates and installs a test extension that overrides the default search
  // provider with the given parameters.
  const extensions::Extension* InstallCustomSearchExtension(
      const std::string& name,
      const std::string& keyword,
      const std::string& search_url,
      std::optional<int> prepopulated_id,
      std::optional<GURL> favicon_url) {
    return AddExtensionControllingSearch(name, keyword, search_url,
                                         /*is_simple_override=*/false,
                                         prepopulated_id, favicon_url);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  unsigned int next_icon_offset_ = 0;
};

// Verify correctness of the search option-agnostic dialog strings.
TEST_F(ExtensionControllingSearchExplicitChoiceParamsUnitTest,
       StaticDialogStrings) {
  const extensions::Extension* extension = AddExtensionControllingSearch();
  ASSERT_TRUE(extension);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);

  EXPECT_TRUE(params->content.dialog_title.contains(u"Confirm"));
  EXPECT_TRUE(params->content.message.contains(
      base::StrCat({u"The extension \"", kSearchOverrideExtensionName16,
                    u"\" changed your settings"})));

  ASSERT_TRUE(params->content.previous_setting.has_value());
  EXPECT_EQ(u"Your previous choice",
            params->content.previous_setting->description);

  ASSERT_TRUE(params->content.new_setting.has_value());
  EXPECT_EQ(u"Changed to this by extension",
            params->content.new_setting->description);
}

TEST_F(ExtensionControllingSearchExplicitChoiceParamsUnitTest,
       PrimarySearchIsExtensionNotInPrepopulatedList) {
  base::HistogramTester histogram_tester;
  const extensions::Extension* extension = AddExtensionControllingSearch();
  ASSERT_TRUE(extension);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);

  auto new_setting = params->content.new_setting;
  ASSERT_TRUE(new_setting.has_value());
  EXPECT_TRUE(IconsEqual(GetExpectedFallbackIcon(), new_setting.value().image));
  EXPECT_EQ(u"example.com", new_setting.value().text);

  // Ensure that a fully-populated dialog was logged.
  histogram_tester.ExpectUniqueSample(
      "Extensions.SettingsOverridden.MissingParams",
      settings_overridden_params::MissingParams::kNone, 1);
}

TEST_F(ExtensionControllingSearchExplicitChoiceParamsUnitTest,
       PrimarySearchIsExtensionInPrepopulatedList) {
  const TemplateURL* extension_turl = FindPrepopulatedTemplateURL();
  EXPECT_FALSE(extension_turl->favicon_url().is_empty());

  // Use a name and favicon URL in the extension manifest that differ from the
  // prepopulated engine's values to verify that the dialog uses the
  // authoritative prepopulated values rather than those from the manifest.
  constexpr char kManifestName[] = "PrimaryPrepopulatedExtension";
  const GURL manifest_favicon_url("https://example.com/manifest_favicon.ico");
  ASSERT_NE(kManifestName, base::UTF16ToUTF8(extension_turl->short_name()));
  ASSERT_NE(manifest_favicon_url, extension_turl->favicon_url());

  const extensions::Extension* extension = InstallCustomSearchExtension(
      kManifestName, base::UTF16ToUTF8(extension_turl->keyword()),
      extension_turl->url(), /*prepopulated_id=*/std::nullopt,
      manifest_favicon_url);
  ASSERT_TRUE(extension);

  const ui::ImageModel expected_icon = SetUpExpectedEngineIcon(*extension_turl);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);

  auto new_setting = params->content.new_setting;
  ASSERT_TRUE(new_setting.has_value());
  EXPECT_EQ(extension_turl->short_name(), new_setting.value().text);
  EXPECT_TRUE(IconsEqual(expected_icon, new_setting.value().image));
}

TEST_F(ExtensionControllingSearchExplicitChoiceParamsUnitTest,
       SecondarySearchIsPrepopulatedNonExtension) {
  TemplateURLService* const template_url_service = GetTemplateURLService();
  const TemplateURL* const default_provider =
      template_url_service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_provider);
  EXPECT_FALSE(default_provider->favicon_url().is_empty());

  // Initially, default search is Google (non-extension, prepopulated).
  const extensions::Extension* extension = AddExtensionControllingSearch();
  ASSERT_TRUE(extension);

  const ui::ImageModel expected_icon =
      SetUpExpectedEngineIcon(*default_provider);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);

  auto previous_setting = params->content.previous_setting;
  ASSERT_TRUE(previous_setting.has_value());
  EXPECT_EQ(u"Google", previous_setting.value().text);
  EXPECT_TRUE(IconsEqual(expected_icon, previous_setting.value().image));
}

TEST_F(ExtensionControllingSearchExplicitChoiceParamsUnitTest,
       SecondarySearchIsNonPrepopulatedNonExtension) {
  // Create and set a custom (non-prepopulated) search provider.
  TemplateURLService* const template_url_service = GetTemplateURLService();
  std::unique_ptr<TemplateURLData> data =
      GenerateDummyTemplateURLData("custom");
  data->SetURL("https://customsearch.com/search?q={searchTerms}");
  data->favicon_url = GURL("https://customsearch.com/favicon.ico");
  TemplateURL* custom_turl =
      template_url_service->Add(std::make_unique<TemplateURL>(*data));
  template_url_service->SetUserSelectedDefaultSearchProvider(custom_turl);

  const extensions::Extension* extension = AddExtensionControllingSearch();
  ASSERT_TRUE(extension);

  const ui::ImageModel expected_icon = SetUpExpectedEngineIcon(*custom_turl);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);

  auto previous_setting = params->content.previous_setting;
  ASSERT_TRUE(previous_setting.has_value());
  // For a non-extension custom search engine, the user's configured short name
  // and fetched icon should be preserved.
  EXPECT_EQ(custom_turl->short_name(), previous_setting.value().text);
  EXPECT_TRUE(IconsEqual(expected_icon, previous_setting.value().image));
}

TEST_F(ExtensionControllingSearchExplicitChoiceParamsUnitTest,
       SecondarySearchIsExtensionInPrepopulatedList) {
  const TemplateURL* extension_turl = FindPrepopulatedTemplateURL();
  EXPECT_FALSE(extension_turl->favicon_url().is_empty());

  // Use a name and favicon URL in the extension manifest that differ from the
  // prepopulated engine's values to verify that the dialog uses the
  // authoritative prepopulated values rather than those from the manifest.
  constexpr char kManifestName[] = "FirstPrepopulatedExtension";
  const GURL manifest_favicon_url("https://example.com/manifest_favicon.ico");
  ASSERT_NE(kManifestName, base::UTF16ToUTF8(extension_turl->short_name()));
  ASSERT_NE(manifest_favicon_url, extension_turl->favicon_url());

  // Install the first extension overriding search with a prepopulated search
  // URL.
  const extensions::Extension* first_extension = InstallCustomSearchExtension(
      kManifestName, base::UTF16ToUTF8(extension_turl->keyword()),
      extension_turl->url(), /*prepopulated_id=*/std::nullopt,
      manifest_favicon_url);
  ASSERT_TRUE(first_extension);

  // Install a second extension, which becomes the primary controlling
  // extension. The first extension becomes the secondary search.
  const extensions::Extension* second_extension =
      AddExtensionControllingSearch();
  ASSERT_TRUE(second_extension);

  const ui::ImageModel expected_icon = SetUpExpectedEngineIcon(*extension_turl);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);

  auto previous_setting = params->content.previous_setting;
  ASSERT_TRUE(previous_setting.has_value());
  EXPECT_EQ(extension_turl->short_name(), previous_setting.value().text);
  EXPECT_TRUE(IconsEqual(expected_icon, previous_setting.value().image));
}

TEST_F(ExtensionControllingSearchExplicitChoiceParamsUnitTest,
       SecondarySearchIsExtensionNotInPrepopulatedList) {
  // Install the first extension with a non-prepopulated search URL.
  const extensions::Extension* first_extension = InstallCustomSearchExtension(
      "FirstNonPrepopulatedExtension", "first_keyword",
      "https://first-override.com/search?q={searchTerms}",
      /*prepopulated_id=*/std::nullopt, /*favicon_url=*/std::nullopt);
  ASSERT_TRUE(first_extension);

  // Install a second extension, which becomes the primary controlling
  // extension. The first extension becomes the secondary search.
  const extensions::Extension* second_extension =
      AddExtensionControllingSearch();
  ASSERT_TRUE(second_extension);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);

  auto previous_setting = params->content.previous_setting;
  ASSERT_TRUE(previous_setting.has_value());
  EXPECT_TRUE(
      IconsEqual(GetExpectedFallbackIcon(), previous_setting.value().image));
  EXPECT_EQ(u"first-override.com", previous_setting.value().text);
}

TEST_F(ExtensionControllingSearchExplicitChoiceParamsUnitTest,
       PrimaryAndSecondarySearchesFetchDistinctIcons) {
  TemplateURLService* const template_url_service = GetTemplateURLService();
  const TemplateURL* const secondary_turl =
      template_url_service->GetDefaultSearchProvider();
  ASSERT_TRUE(secondary_turl);

  const TemplateURL* const primary_turl = FindPrepopulatedTemplateURL();
  ASSERT_TRUE(primary_turl);

  const GURL manifest_favicon_url("https://example.com/manifest_favicon.ico");
  const extensions::Extension* extension = InstallCustomSearchExtension(
      "PrimaryPrepopulatedExtension",
      base::UTF16ToUTF8(primary_turl->keyword()), primary_turl->url(),
      primary_turl->prepopulate_id(), manifest_favicon_url);
  ASSERT_TRUE(extension);

  ExpectNoIconFetch(manifest_favicon_url);
  const ui::ImageModel expected_secondary_icon =
      SetUpExpectedEngineIcon(*secondary_turl);
  const ui::ImageModel expected_primary_icon =
      SetUpExpectedEngineIcon(*primary_turl);
  ASSERT_FALSE(IconsEqual(expected_secondary_icon, expected_primary_icon));

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);

  auto previous_setting = params->content.previous_setting;
  ASSERT_TRUE(previous_setting.has_value());
  auto new_setting = params->content.new_setting;
  ASSERT_TRUE(new_setting.has_value());

  EXPECT_TRUE(IconsEqual(expected_secondary_icon, previous_setting->image));
  EXPECT_TRUE(IconsEqual(expected_primary_icon, new_setting->image));
  EXPECT_FALSE(IconsEqual(previous_setting->image, new_setting->image));
}

// Verifies that a prepopulated search engine without a built-in resource icon
// (such as Baidu) falls back to fetching its favicon over the network.
TEST_F(ExtensionControllingSearchExplicitChoiceParamsUnitTest,
       PrepopulatedEngineWithoutBuiltinResourceFallsBackToNetworkFetch) {
  ASSERT_FALSE(
      GetExpectedBuiltinIcon(TemplateURLPrepopulateData::baidu).has_value());
  const GURL baidu_favicon_url(TemplateURLPrepopulateData::baidu.favicon_url);
  EXPECT_FALSE(baidu_favicon_url.is_empty());

  const extensions::Extension* baidu_extension = InstallCustomSearchExtension(
      "BaiduOverrideExtension", "baidu_ext",
      "https://www.baidu.com/s?wd={searchTerms}",
      /*prepopulated_id=*/std::nullopt,
      GURL("https://example.com/custom_baidu_favicon.ico"));
  ASSERT_TRUE(baidu_extension);

  const ui::ImageModel expected_baidu_icon =
      SetUpExpectedEngineIcon(TemplateURLPrepopulateData::baidu);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);

  auto new_setting = params->content.new_setting;
  ASSERT_TRUE(new_setting.has_value());
  EXPECT_EQ(TemplateURLPrepopulateData::baidu.name, new_setting.value().text);
  EXPECT_TRUE(IconsEqual(expected_baidu_icon, new_setting.value().image));
}

// Verifies that when multiple prepopulated search engines share the same
// SearchEngineType (such as regional Yahoo variants), the dialog resolves the
// variant matching the profile's region rather than the first entry in the
// global list.
TEST_F(ExtensionControllingSearchExplicitChoiceParamsUnitTest,
       PrepopulatedEngineResolvesRegionalVariant) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "FR");
  ASSERT_NE(TemplateURLPrepopulateData::yahoo.name,
            TemplateURLPrepopulateData::yahoo_fr.name);

  const extensions::Extension* yahoo_extension = InstallCustomSearchExtension(
      "YahooOverrideExtension", "yahoo_ext",
      "https://fr.search.yahoo.com/search?p={searchTerms}",
      /*prepopulated_id=*/std::nullopt,
      GURL("https://example.com/custom_yahoo_favicon.ico"));
  ASSERT_TRUE(yahoo_extension);

  const ui::ImageModel expected_yahoo_icon =
      SetUpExpectedEngineIcon(TemplateURLPrepopulateData::yahoo_fr);

  std::optional<ExtensionSettingsOverriddenDialog::Params> params =
      GetSearchOverriddenParamsSync();
  ASSERT_TRUE(params);

  auto new_setting = params->content.new_setting;
  ASSERT_TRUE(new_setting.has_value());
  EXPECT_EQ(TemplateURLPrepopulateData::yahoo_fr.name,
            new_setting.value().text);
  EXPECT_TRUE(IconsEqual(expected_yahoo_icon, new_setting.value().image));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
