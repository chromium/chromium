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
void InstallExtensionControlledDse(TestingProfile* profile,
                                   TemplateURLService& service,
                                   const extensions::Extension& extension) {
  TemplateURLData data;
  data.SetShortName(u"Ext Search");
  data.SetKeyword(u"ext");
  data.SetURL("https://www.example.com/?q={searchTerms}");

  auto turl = std::make_unique<TemplateURL>(
      data, TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION, extension.id(),
      base::Time::Now(), /*wants_to_be_default_engine=*/true);

  TemplateURL* added = service.Add(std::move(turl));
  ASSERT_TRUE(added);

  SetExtensionDefaultSearchInPrefs(profile->GetTestingPrefService(),
                                   added->data());

  const TemplateURL* dse = service.GetDefaultSearchProvider();
  ASSERT_TRUE(dse);
  ASSERT_EQ(dse->type(), TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION);
  ASSERT_EQ(dse->GetExtensionId(), extension.id());
}

}  // namespace

class DefaultSearchExtensionControlledControllerUnitTest
    : public testing::Test {
 protected:
  DefaultSearchExtensionControlledControllerUnitTest() {
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

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<TemplateURLService> template_url_service_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DefaultSearchExtensionControlledControllerUnitTest,
       ShouldRequestConfirmation_True_ForGeneratedSearchUrl) {
  auto extension =
      AddEnabledExtension(&profile_, "Test Extension", kExtensionId);

  InstallExtensionControlledDse(&profile_, *template_url_service_, *extension);

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  GURL search_url("https://www.example.com/?q=foo");
  EXPECT_TRUE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerUnitTest,
       ShouldRequestConfirmation_False_ForSimpleOverrideExtension) {
  // Use the Simple extension helper (no extra permissions).
  auto extension =
      AddSimpleEnabledExtension(&profile_, "Simple Extension", kExtensionId);

  InstallExtensionControlledDse(&profile_, *template_url_service_, *extension);

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  GURL search_url("https://www.example.com/?q=foo");
  // Simple overrides should be ignored by the controller.
  EXPECT_FALSE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerUnitTest,
       ShouldRequestConfirmationFalseWhenUrlIsNotDseSearchUrl) {
  auto extension =
      AddEnabledExtension(&profile_, "Test Extension", kExtensionId);

  InstallExtensionControlledDse(&profile_, *template_url_service_, *extension);

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  EXPECT_FALSE(owned_controller.ShouldRequestConfirmationForExtensionDse(
      GURL("https://www.example.com/about")));
}

TEST_F(DefaultSearchExtensionControlledControllerUnitTest,
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

TEST_F(DefaultSearchExtensionControlledControllerUnitTest,
       ShouldRequestConfirmationFalseWhenAlreadyAcknowledged) {
  auto extension =
      AddEnabledExtension(&profile_, "Test Extension", kExtensionId);

  InstallExtensionControlledDse(&profile_, *template_url_service_, *extension);

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

TEST_F(DefaultSearchExtensionControlledControllerUnitTest,
       ShouldRequestConfirmationFalseWhenDialogAlreadyShown) {
  auto extension =
      AddEnabledExtension(&profile_, "Test Extension", kExtensionId);
  InstallExtensionControlledDse(&profile_, *template_url_service_, *extension);

  ExtensionSettingsOverriddenDialog::Params params(
      extension->id(), ControlledHomeDialogController::kAcknowledgedPreference,
      "Test.Histogram",
      ExtensionSettingsOverriddenDialog::ShowParams(u"Title", u"Body",
                                                    nullptr));
  ExtensionSettingsOverriddenDialog dialog(std::move(params), &profile_);
  dialog.OnDialogShown();

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  GURL search_url("https://www.example.com/?q=foo");
  EXPECT_FALSE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerUnitTest,
       ShouldRequestConfirmationFalseWhenExtensionIsForceInstalled) {
  auto extension =
      AddPolicyEnabledExtension(&profile_, "Policy Extension", kExtensionId);

  InstallExtensionControlledDse(&profile_, *template_url_service_, *extension);

  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  GURL search_url(
      template_url_service_->GenerateSearchURLForDefaultSearchProvider(u"foo"));
  ASSERT_TRUE(search_url.is_valid());

  // Force-installed extensions should not trigger the choice dialog.
  EXPECT_FALSE(
      owned_controller.ShouldRequestConfirmationForExtensionDse(search_url));
}

TEST_F(DefaultSearchExtensionControlledControllerUnitTest,
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

TEST_F(DefaultSearchExtensionControlledControllerUnitTest,
       FromReturnsInstanceAttachedToWindow) {
  DefaultSearchExtensionControlledController owned_controller(browser_window_,
                                                              profile_);

  // Verify that From() retrieves the instance we just created and attached
  // via scoped_unowned_user_data_.
  EXPECT_EQ(&owned_controller,
            DefaultSearchExtensionControlledController::From(&browser_window_));
}
