// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_translation_manager.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

class WebAppTranslationManagerTest : public WebAppTest {
  void SetUp() override {
    WebAppTest::SetUp();

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    fake_registry_controller_->SetUp(profile());

    file_utils_ = base::MakeRefCounted<TestFileUtils>();

    controller().Init();
    InitWebAppProvider();
  }

 protected:
  void AwaitWriteTranslations(
      const AppId& app_id,
      const base::flat_map<Locale, blink::Manifest::TranslationItem>&
          translations) {
    base::RunLoop run_loop;
    translation_manager().WriteTranslations(
        app_id, translations, base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  void AwaitDeleteTranslations(const AppId& app_id) {
    base::RunLoop run_loop;
    translation_manager().DeleteTranslations(
        app_id, base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  std::map<AppId, blink::Manifest::TranslationItem> AwaitReadTranslations() {
    base::RunLoop run_loop;
    std::map<AppId, blink::Manifest::TranslationItem> result;
    translation_manager().ReadTranslations(base::BindLambdaForTesting(
        [&](const std::map<AppId, blink::Manifest::TranslationItem>& cache) {
          result = std::move(cache);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void InitWebAppProvider() {
    provider_ = web_app::FakeWebAppProvider::Get(profile());
    provider_->SetOsIntegrationManager(
        std::make_unique<FakeOsIntegrationManager>(profile(), nullptr, nullptr,
                                                   nullptr, nullptr));
    // FakeWebAppProvider should not wait for a test extension system, that is
    // never started, to be ready.
    provider_->SkipAwaitingExtensionSystem();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }

  WebAppInstallManager& install_manager() { return *install_manager_; }

  FakeWebAppProvider& provider() { return *provider_; }
  WebAppRegistrar& registrar() { return controller().registrar(); }
  WebAppTranslationManager& translation_manager() {
    return controller().translation_manager();
  }
  TestFileUtils& file_utils() {
    DCHECK(file_utils_);
    return *file_utils_;
  }

 private:
  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  scoped_refptr<TestFileUtils> file_utils_;
  web_app::FakeWebAppProvider* provider_;
  base::test::ScopedFeatureList features_{
      blink::features::kWebAppEnableTranslations};
};

TEST_F(WebAppTranslationManagerTest, WriteReadAndDelete) {
  auto web_app1 = test::CreateWebApp(GURL("https://example.com/path"));
  const AppId app_id1 = web_app1->app_id();
  web_app1->SetName("App1 name");
  controller().RegisterApp(std::move(web_app1));

  auto web_app2 = test::CreateWebApp(GURL("https://example.com/path2"));
  const AppId app_id2 = web_app2->app_id();
  web_app2->SetName("App2 name");
  controller().RegisterApp(std::move(web_app2));

  g_browser_process->SetApplicationLocale("en");

  base::flat_map<Locale, blink::Manifest::TranslationItem> translations1;
  base::flat_map<Locale, blink::Manifest::TranslationItem> translations2;

  blink::Manifest::TranslationItem item1;
  item1.name = "name 1";
  item1.short_name = "short name 1";
  item1.description = "description 1";
  translations1[u"en"] = item1;

  blink::Manifest::TranslationItem item2;
  item2.name = "name 2";
  item2.description = "description 2";
  translations1[u"fr"] = item2;

  blink::Manifest::TranslationItem item3;
  item3.short_name = "short name 3";
  translations2[u"en"] = item3;

  blink::Manifest::TranslationItem item4;
  item4.short_name = "short name 4";
  item4.description = "description 4";
  translations2[u"fr"] = item4;

  // Write translations for both apps.
  AwaitWriteTranslations(app_id1, translations1);
  AwaitWriteTranslations(app_id2, translations2);

  // Read translations for the current language.
  {
    std::map<AppId, blink::Manifest::TranslationItem> cache =
        AwaitReadTranslations();
    ASSERT_EQ(cache.size(), static_cast<size_t>(2));
    EXPECT_EQ(cache.find(app_id1)->second, item1);
    EXPECT_EQ(cache.find(app_id2)->second, item3);

    EXPECT_EQ(translation_manager().GetTranslatedName(app_id1), item1.name);
    EXPECT_EQ(translation_manager().GetTranslatedDescription(app_id1),
              item1.description);

    EXPECT_EQ(registrar().GetAppShortName(app_id1), item1.name);
    EXPECT_EQ(registrar().GetAppDescription(app_id1), item1.description);

    EXPECT_EQ(translation_manager().GetTranslatedName(app_id2), "");
    EXPECT_EQ(translation_manager().GetTranslatedDescription(app_id2), "");

    EXPECT_EQ(registrar().GetAppShortName(app_id2), "App2 name");
    EXPECT_EQ(registrar().GetAppDescription(app_id2), "");
  }

  // Delete translations for web_app1.
  AwaitDeleteTranslations(app_id1);

  EXPECT_EQ(translation_manager().GetTranslatedName(app_id1), "");
  EXPECT_EQ(translation_manager().GetTranslatedDescription(app_id1), "");

  EXPECT_EQ(registrar().GetAppShortName(app_id1), "App1 name");
  EXPECT_EQ(registrar().GetAppDescription(app_id1), "");

  EXPECT_EQ(translation_manager().GetTranslatedName(app_id2), "");
  EXPECT_EQ(translation_manager().GetTranslatedDescription(app_id2), "");

  EXPECT_EQ(registrar().GetAppShortName(app_id2), "App2 name");
  EXPECT_EQ(registrar().GetAppDescription(app_id2), "");

  // Read translations to ensure web_app1 deleted.
  {
    std::map<AppId, blink::Manifest::TranslationItem> cache =
        AwaitReadTranslations();
    ASSERT_EQ(cache.size(), static_cast<size_t>(1));
    EXPECT_EQ(cache.find(app_id2)->second, item3);
  }
}

TEST_F(WebAppTranslationManagerTest, UpdateTranslations) {
  auto web_app1 = test::CreateWebApp(GURL("https://example.com/path"));
  const AppId app_id1 = web_app1->app_id();
  controller().RegisterApp(std::move(web_app1));

  g_browser_process->SetApplicationLocale("en");

  base::flat_map<Locale, blink::Manifest::TranslationItem> translations1;
  base::flat_map<Locale, blink::Manifest::TranslationItem> translations2;

  blink::Manifest::TranslationItem item1;
  item1.name = "name 1";
  item1.short_name = "short name 1";
  item1.description = "description 1";
  translations1[u"en"] = item1;

  blink::Manifest::TranslationItem item2;
  item2.name = "name 2";
  item2.description = "description 2";
  translations2[u"en"] = item2;

  // Write translations for the app.
  AwaitWriteTranslations(app_id1, translations1);

  // Check the translations set correctly.
  EXPECT_EQ(translation_manager().GetTranslatedName(app_id1), item1.name);
  EXPECT_EQ(translation_manager().GetTranslatedDescription(app_id1),
            item1.description);

  EXPECT_EQ(registrar().GetAppShortName(app_id1), item1.name);
  EXPECT_EQ(registrar().GetAppDescription(app_id1), item1.description);

  // Update the translations for the app.
  AwaitWriteTranslations(app_id1, translations2);

  // Check the translations have correctly updated.
  EXPECT_EQ(translation_manager().GetTranslatedName(app_id1), item2.name);
  EXPECT_EQ(translation_manager().GetTranslatedDescription(app_id1),
            item2.description);

  EXPECT_EQ(registrar().GetAppShortName(app_id1), item2.name);
  EXPECT_EQ(registrar().GetAppDescription(app_id1), item2.description);
}

TEST_F(WebAppTranslationManagerTest, InstallAndUninstall) {
  g_browser_process->SetApplicationLocale("en");

  base::flat_map<Locale, blink::Manifest::TranslationItem> translations;

  blink::Manifest::TranslationItem item1;
  item1.name = "name 1";
  item1.short_name = "short name 1";
  item1.description = "description 1";
  translations[u"en"] = item1;

  auto app_info = std::make_unique<WebAppInstallInfo>();
  app_info->start_url = GURL("https://example.com/path");
  app_info->scope = GURL("https://example.com/path");
  app_info->title = u"Web App";
  app_info->translations = translations;

  // Install app
  AppId app_id = web_app::test::InstallWebApp(profile(), std::move(app_info));

  // Check translations are stored
  EXPECT_EQ(provider().translation_manager().GetTranslatedName(app_id),
            item1.name);

  // Uninstall app
  web_app::test::UninstallWebApp(profile(), app_id);

  // Check translations were deleted
  EXPECT_EQ(provider().translation_manager().GetTranslatedName(app_id),
            std::string());
}

// TODO(crbug.com/1259777): Add a test for an app which is installed before the
// translation manager is started.

}  // namespace web_app
