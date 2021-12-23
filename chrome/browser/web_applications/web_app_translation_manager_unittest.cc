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

    translation_manager_ = std::make_unique<WebAppTranslationManager>(
        profile(), registrar(), file_utils_);
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

  FakeWebAppProvider& provider() { return *provider_; }

  WebAppRegistrar* registrar() { return &provider().registrar(); }
  WebAppTranslationManager& translation_manager() {
    return *translation_manager_;
  }
  TestFileUtils& file_utils() {
    DCHECK(file_utils_);
    return *file_utils_;
  }

 private:
  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<WebAppTranslationManager> translation_manager_;
  scoped_refptr<TestFileUtils> file_utils_;
  web_app::FakeWebAppProvider* provider_;
};

TEST_F(WebAppTranslationManagerTest, WriteReadAndDelete) {
  auto web_app1 = test::CreateWebApp(GURL("https://example.com/path"));
  const AppId app_id1 = web_app1->app_id();
  auto web_app2 = test::CreateWebApp(GURL("https://example.com/path2"));
  const AppId app_id2 = web_app2->app_id();

  g_browser_process->SetApplicationLocale("en");

  base::flat_map<Locale, blink::Manifest::TranslationItem> translations1;
  base::flat_map<Locale, blink::Manifest::TranslationItem> translations2;

  blink::Manifest::TranslationItem item1;
  item1.name = u"name 1";
  item1.short_name = u"short name 1";
  item1.description = u"description 1";
  translations1[u"en"] = item1;

  blink::Manifest::TranslationItem item2;
  item2.name = u"name 2";
  item2.description = u"description 2";
  translations1[u"fr"] = item2;

  blink::Manifest::TranslationItem item3;
  item3.name = u"name 3";
  translations2[u"en"] = item3;

  blink::Manifest::TranslationItem item4;
  item4.short_name = u"short name 4";
  item4.description = u"description 4";
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
  }

  // Delete translations for web_app1.
  AwaitDeleteTranslations(app_id1);

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

  g_browser_process->SetApplicationLocale("en");

  base::flat_map<Locale, blink::Manifest::TranslationItem> translations1;
  base::flat_map<Locale, blink::Manifest::TranslationItem> translations2;

  blink::Manifest::TranslationItem item1;
  item1.name = u"name 1";
  item1.short_name = u"short name 1";
  item1.description = u"description 1";
  translations1[u"en"] = item1;

  blink::Manifest::TranslationItem item2;
  item2.name = u"name 2";
  item2.description = u"description 2";
  translations2[u"en"] = item2;

  // Write translations for the app.
  AwaitWriteTranslations(app_id1, translations1);

  // Update the translations for the app.
  AwaitWriteTranslations(app_id1, translations2);

  // Check the translations have correctly updated.
  std::map<AppId, blink::Manifest::TranslationItem> cache =
      AwaitReadTranslations();
  ASSERT_EQ(cache.size(), static_cast<size_t>(1));
  EXPECT_EQ(cache.find(app_id1)->second, item2);
}

// TODO(crbug.com/1259777): Add a test for installing and uninstalling an app.

// TODO(crbug.com/1259777): Add a test for an app which is installed before the
// translation manager is started.

}  // namespace web_app
