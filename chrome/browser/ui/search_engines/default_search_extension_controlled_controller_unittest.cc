// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/default_search_extension_controlled_controller.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"
#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_pref_names.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

namespace {

constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId2[] = "bcdefghijklmnopabcdefghijklmnopa";

// Adds a "Complex" extension (has extra permissions) to ensure it is NOT
// treated as a simple override.
scoped_refptr<const extensions::Extension> AddEnabledExtension(
    Profile* profile,
    const std::string& name,
    const std::string& id) {
  // Add "permissions" to make the extension "complex" so
  // IsSimpleOverrideExtension returns false. This ensures the controller logic
  // proceeds to request confirmation.
  constexpr char kManifest[] = R"({
    "name": "Test Extension",
    "version": "1.0",
    "manifest_version": 2,
    "permissions": ["storage"],
    "chrome_settings_overrides": {
      "search_provider": {
        "name": "Test Search",
        "keyword": "test",
        "search_url": "https://www.example.com/?q={searchTerms}",
        "favicon_url": "https://www.example.com/favicon.ico",
        "encoding": "UTF-8",
        "is_default": true
      }
    }
  })";

  auto extension =
      extensions::ExtensionBuilder()
          .SetManifest(base::test::ParseJsonDict(kManifest))
          .SetID(id)
          .SetLocation(extensions::mojom::ManifestLocation::kInternal)
          .Build();
  CHECK(extension);

  auto* registry = extensions::ExtensionRegistry::Get(profile);
  CHECK(registry);
  registry->AddEnabled(extension);
  return extension;
}

// Adds a "Simple" extension (no extra permissions). Simple overrides are
// skipped by the controller.
scoped_refptr<const extensions::Extension> AddSimpleEnabledExtension(
    Profile* profile,
    const std::string& name,
    const std::string& id) {
  constexpr char kManifest[] = R"({
    "name": "Simple Extension",
    "version": "1.0",
    "manifest_version": 2,
    "chrome_settings_overrides": {
      "search_provider": {
        "name": "Test Search",
        "keyword": "test",
        "search_url": "https://www.example.com/?q={searchTerms}",
        "favicon_url": "https://www.example.com/favicon.ico",
        "encoding": "UTF-8",
        "is_default": true
      }
    }
  })";

  auto extension =
      extensions::ExtensionBuilder()
          .SetManifest(base::test::ParseJsonDict(kManifest))
          .SetID(id)
          .SetLocation(extensions::mojom::ManifestLocation::kInternal)
          .Build();
  CHECK(extension);

  auto* registry = extensions::ExtensionRegistry::Get(profile);
  CHECK(registry);
  registry->AddEnabled(extension);
  return extension;
}

// Adds a Policy-installed extension. These must remain enabled and should not
// trigger the dialog.
scoped_refptr<const extensions::Extension> AddPolicyEnabledExtension(
    Profile* profile,
    const std::string& name,
    const std::string& id) {
  constexpr char kManifest[] = R"({
    "name": "Policy Extension",
    "version": "1.0",
    "manifest_version": 2,
    "permissions": ["storage"],
    "chrome_settings_overrides": {
      "search_provider": {
        "name": "Test Search",
        "keyword": "test",
        "search_url": "https://www.example.com/?q={searchTerms}",
        "favicon_url": "https://www.example.com/favicon.ico",
        "encoding": "UTF-8",
        "is_default": true
      }
    }
  })";

  auto extension =
      extensions::ExtensionBuilder()
          .SetManifest(base::test::ParseJsonDict(kManifest))
          .SetID(id)
          .SetLocation(extensions::mojom::ManifestLocation::kExternalPolicy)
          .Build();
  CHECK(extension);

  auto* registry = extensions::ExtensionRegistry::Get(profile);
  CHECK(registry);
  registry->AddEnabled(extension);
  return extension;
}

// Creates an extension-controlled DSE and sets it as default.
void InstallExtensionControlledDse(
    TestingProfile& profile,
    TemplateURLService& service,
    const extensions::Extension& extension,
    const std::u16string& keyword = u"ext",
    const std::string& url_str = "https://www.example.com/?q={searchTerms}") {
  TemplateURLData data;
  data.SetShortName(u"Ext Search");
  data.SetKeyword(keyword);
  data.SetURL(url_str);

  auto turl = std::make_unique<TemplateURL>(
      data, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, extension.id(),
      base::Time::Now(), /*wants_to_be_default_engine=*/true);

  TemplateURL* added = service.Add(std::move(turl));
  ASSERT_TRUE(added);

  SetExtensionDefaultSearchInPrefs(profile.GetTestingPrefService(),
                                   added->data());

  const TemplateURL* dse = service.GetDefaultSearchProvider();
  ASSERT_TRUE(dse);
  ASSERT_EQ(dse->type(), TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION);
  ASSERT_EQ(dse->GetExtensionId(), extension.id());

  // The helpers used in this test to install extensions register them directly
  // in the registry. Therefore, the Prefs is not populated. As a result
  // `extension::GetFirstInstallTime()` will return null Time. Set it here to
  // ensure that the test is deterministic under mock time.
  extensions::ExtensionPrefs::Get(&profile)->UpdateExtensionPref(
      extension.id(), extensions::kPrefFirstInstallTime,
      base::Value(base::NumberToString(
          base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds())));
}

}  // namespace

class DefaultSearchExtensionControlledControllerTest : public testing::Test {
 protected:
  DefaultSearchExtensionControlledControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kSearchEngineExplicitChoiceDialog);
  }

  void SetUp() override {
    // Initialize ExtensionSystem first so it's ready when TemplateURLService
    // is created.
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&profile_));
    extension_system->InitForRegularProfile(/*extensions_enabled=*/true);

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_,
        TemplateURLServiceTestUtil::GetTemplateURLServiceTestingFactory());

    template_url_service_ = TemplateURLServiceFactory::GetForProfile(&profile_);
    ASSERT_TRUE(template_url_service_);
    template_url_service_->Load();

    ON_CALL(browser_window_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
    ON_CALL(testing::Const(browser_window_), GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  raw_ptr<TemplateURLService> template_url_service_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DefaultSearchExtensionControlledControllerTest,
       ShouldRequestConfirmationTrueForGeneratedSearchUrl) {
  auto extension =
      AddEnabledExtension(&profile_, "Test Extension", kExtensionId);

  InstallExtensionControlledDse(profile_, *template_url_service_, *extension);

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  GURL search_url("https://www.example.com/?q=foo");
  EXPECT_TRUE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerTest,
       ShouldRequestConfirmationFalseForSimpleOverrideExtension) {
  // Use the Simple extension helper (no extra permissions).
  auto extension =
      AddSimpleEnabledExtension(&profile_, "Simple Extension", kExtensionId);

  InstallExtensionControlledDse(profile_, *template_url_service_, *extension);

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  GURL search_url("https://www.example.com/?q=foo");
  // Simple overrides should be ignored by the controller.
  EXPECT_FALSE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerTest,
       ShouldRequestConfirmationFalseWhenUrlIsNotDseSearchUrl) {
  auto extension =
      AddEnabledExtension(&profile_, "Test Extension", kExtensionId);

  InstallExtensionControlledDse(profile_, *template_url_service_, *extension);

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  EXPECT_FALSE(owned_controller.ShouldRequestConfirmationForExtensionDse(
      GURL("https://www.example.com/about")));
}

TEST_F(DefaultSearchExtensionControlledControllerTest,
       ShouldRequestConfirmationFalseWhenExtensionNotEnabled) {
  // Create DSE controlled by kExtensionId, but do NOT add extension to
  // registry.
  TemplateURLData data;
  data.SetShortName(u"Ext Search");
  data.SetKeyword(u"ext");
  data.SetURL("https://www.example.com/?q={searchTerms}");

  auto turl = std::make_unique<TemplateURL>(
      data, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, kExtensionId,
      base::Time::Now(), /*wants_to_be_default_engine=*/true);

  TemplateURL* added = template_url_service_->Add(std::move(turl));
  ASSERT_TRUE(added);
  SetExtensionDefaultSearchInPrefs(profile_.GetTestingPrefService(),
                                   added->data());

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  GURL search_url(
      template_url_service_->GenerateSearchURLForDefaultSearchProvider(u"foo"));
  ASSERT_TRUE(search_url.is_valid());

  EXPECT_FALSE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerTest,
       ShouldRequestConfirmationFalseWhenAlreadyAcknowledged) {
  auto extension =
      AddEnabledExtension(&profile_, "Test Extension", kExtensionId);

  InstallExtensionControlledDse(profile_, *template_url_service_, *extension);

  extensions::ExtensionPrefs::Get(&profile_)->UpdateExtensionPref(
      extension->id(), ControlledHomeDialogController::kAcknowledgedPreference,
      base::Value(true));

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  GURL search_url(
      template_url_service_->GenerateSearchURLForDefaultSearchProvider(u"foo"));
  ASSERT_TRUE(search_url.is_valid());

  EXPECT_FALSE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerTest,
       ShouldRequestConfirmationFalseWhenDialogAlreadyShown) {
  auto extension =
      AddEnabledExtension(&profile_, "Test Extension", kExtensionId);
  InstallExtensionControlledDse(profile_, *template_url_service_, *extension);

  ExtensionSettingsOverriddenDialog::Params params(
      extension->id(), ControlledHomeDialogController::kAcknowledgedPreference,
      "Test.Histogram",
      ExtensionSettingsOverriddenDialog::ShowParams(u"Title", u"Body",
                                                    nullptr));
  ExtensionSettingsOverriddenDialog dialog(std::move(params), profile_);
  dialog.OnDialogShown();

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  GURL search_url("https://www.example.com/?q=foo");
  EXPECT_FALSE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerTest,
       ShouldRequestConfirmationFalseWhenExtensionIsForceInstalled) {
  auto extension =
      AddPolicyEnabledExtension(&profile_, "Policy Extension", kExtensionId);

  InstallExtensionControlledDse(profile_, *template_url_service_, *extension);

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  GURL search_url(
      template_url_service_->GenerateSearchURLForDefaultSearchProvider(u"foo"));
  ASSERT_TRUE(search_url.is_valid());

  // Force-installed extensions should not trigger the choice dialog.
  EXPECT_FALSE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerTest,
       ShouldRequestConfirmationTrueForSimpleOverrideFeatureEnabledNewInstall) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {extensions_features::kSearchEngineExplicitChoiceDialog,
       extensions_features::kSearchEngineUnconditionalDialog},
      {});

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  // 1. Install an extension at T=0.
  auto extension =
      AddSimpleEnabledExtension(&profile_, "Initial", kExtensionId);
  InstallExtensionControlledDse(profile_, *template_url_service_, *extension);

  // 2. Advance time to T=1s and trigger logic to set enforcement_time = T=1s.
  task_environment_.FastForwardBy(base::Seconds(1));
  GURL search_url1("https://www.example.com/?q=foo");
  // Grandfathered because install_time (T=0) < enforcement_time (T=1s).
  EXPECT_FALSE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url1));

  // 3. Advance time to T=1m and install a NEW extension.
  task_environment_.FastForwardBy(base::Minutes(1));
  auto new_extension =
      AddSimpleEnabledExtension(&profile_, "New Simple", kExtensionId2);
  std::string new_url = "https://www.new-example.com/?q={searchTerms}";
  InstallExtensionControlledDse(profile_, *template_url_service_,
                                *new_extension, u"new-ext", new_url);

  // 4. Check confirmation. New install_time (T=1m1s) >= enforcement_time
  // (T=1s).
  GURL search_url2("https://www.new-example.com/?q=foo");
  EXPECT_TRUE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url2));
}

TEST_F(
    DefaultSearchExtensionControlledControllerTest,
    ShouldRequestConfirmationFalseForSimpleOverrideFeatureEnabledGrandfathered) {
  // Enable the UnconditionalDialog feature.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/
      {extensions_features::kSearchEngineExplicitChoiceDialog,
       extensions_features::kSearchEngineUnconditionalDialog},
      /*disabled_features=*/{});

  // Install the extension *before* the controller logic ever runs.
  auto extension =
      AddSimpleEnabledExtension(&profile_, "Simple Extension", kExtensionId);
  InstallExtensionControlledDse(profile_, *template_url_service_, *extension);

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);
  GURL search_url("https://www.example.com/?q=foo");

  // Advance time slightly to differentiate install time from "Now" when check
  // runs.
  task_environment_.FastForwardBy(base::Seconds(1));

  // The first call sets the enforcement timestamp to Now.
  // Since the extension was installed in the past, it is grandfathered.
  EXPECT_FALSE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerTest,
       ShouldRequestConfirmationFalseWhenDseIsNotExtensionControlled) {
  // Install a normal (non-extension) DSE.
  TemplateURLData data;
  data.SetShortName(u"Google");
  data.SetKeyword(u"google");
  data.SetURL("https://google.com/?q={searchTerms}");

  TemplateURL* added = template_url_service_->Add(
      std::make_unique<TemplateURL>(data, TemplateURL::NORMAL));
  template_url_service_->SetUserSelectedDefaultSearchProvider(added);

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  GURL search_url("https://google.com/?q=foo");
  EXPECT_FALSE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerTest,
       FromReturnsInstanceAttachedToWindow) {
  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  // Verify that From() retrieves the instance we just created and attached
  // via scoped_unowned_user_data_.
  EXPECT_EQ(&owned_controller,
            DefaultSearchExtensionControlledController::From(&browser_window_));
}
