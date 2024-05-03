// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_translation_manager.h"

#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace web_app {

class WebAppTranslationManagerTest : public WebAppTest {
  void SetUp() override {
    WebAppTest::SetUp();

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  void AwaitWriteTranslations(
      const webapps::AppId& app_id,
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

  void AwaitDeleteTranslations(const webapps::AppId& app_id) {
    base::RunLoop run_loop;
    translation_manager().DeleteTranslations(
        app_id, base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  std::map<webapps::AppId, blink::Manifest::TranslationItem>
  AwaitReadTranslations() {
    base::RunLoop run_loop;
    std::map<webapps::AppId, blink::Manifest::TranslationItem> result;
    translation_manager().ReadTranslations(base::BindLambdaForTesting(
        [&](const std::map<webapps::AppId, blink::Manifest::TranslationItem>&
                cache) {
          result = cache;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }
  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }
  WebAppTranslationManager& translation_manager() {
    return provider().translation_manager();
  }

 private:
  base::test::ScopedFeatureList features_{
      blink::features::kWebAppEnableTranslations};
};

TEST_F(WebAppTranslationManagerTest, WriteReadAndDelete) {
  auto app_info1 = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/path"));
  app_info1->title = u"App1 name";
  const webapps::AppId app_id1 =
      test::InstallWebApp(profile(), std::move(app_info1));

  auto app_info2 = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/path2"));
  app_info2->title = u"App2 name";
  const webapps::AppId app_id2 =
      test::InstallWebApp(profile(), std::move(app_info2));

  g_browser_process->SetApplicationLocale("en");

  base::flat_map<Locale, blink::Manifest::TranslationItem> translations1;
  base::flat_map<Locale, blink::Manifest::TranslationItem> translations2;

  blink::Manifest::TranslationItem item1;
  item1.name = "name 1";
  item1.short_name = "short name 1";
  item1.description = "description 1";
  translations1["en"] = item1;

  blink::Manifest::TranslationItem item2;
  item2.name = "name 2";
  item2.description = "description 2";
  translations1["fr"] = item2;

  blink::Manifest::TranslationItem item3;
  item3.short_name = "short name 3";
  translations2["en"] = item3;

  blink::Manifest::TranslationItem item4;
  item4.short_name = "short name 4";
  item4.description = "description 4";
  translations2["fr"] = item4;

  // Write translations for both apps.
  AwaitWriteTranslations(app_id1, translations1);
  AwaitWriteTranslations(app_id2, translations2);

  // Read translations for the current language.
  {
    std::map<webapps::AppId, blink::Manifest::TranslationItem> cache =
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
    std::map<webapps::AppId, blink::Manifest::TranslationItem> cache =
        AwaitReadTranslations();
    ASSERT_EQ(cache.size(), static_cast<size_t>(1));
    EXPECT_EQ(cache.find(app_id2)->second, item3);
  }
}

TEST_F(WebAppTranslationManagerTest, UpdateTranslations) {
  auto app_info1 = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/path"));
  app_info1->title = u"App1 name";
  const webapps::AppId app_id1 =
      test::InstallWebApp(profile(), std::move(app_info1));

  g_browser_process->SetApplicationLocale("en");

  base::flat_map<Locale, blink::Manifest::TranslationItem> translations1;
  base::flat_map<Locale, blink::Manifest::TranslationItem> translations2;

  blink::Manifest::TranslationItem item1;
  item1.name = "name 1";
  item1.short_name = "short name 1";
  item1.description = "description 1";
  translations1["en"] = item1;

  blink::Manifest::TranslationItem item2;
  item2.name = "name 2";
  item2.description = "description 2";
  translations2["en"] = item2;

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
  translations["en"] = item1;

  auto app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/path"));
  app_info->scope = GURL("https://example.com/path");
  app_info->title = u"Web App";
  app_info->translations = translations;

  // Install app
  webapps::AppId app_id = test::InstallWebApp(profile(), std::move(app_info));

  // Check translations are stored
  EXPECT_EQ(provider().translation_manager().GetTranslatedName(app_id),
            item1.name);

  // Uninstall app
  test::UninstallWebApp(profile(), app_id);

  // Check translations were deleted
  EXPECT_EQ(provider().translation_manager().GetTranslatedName(app_id),
            std::string());
}

// TODO(crbug.com/40201597): Add a test for an app which is installed before the
// translation manager is started.

}  // namespace web_app
