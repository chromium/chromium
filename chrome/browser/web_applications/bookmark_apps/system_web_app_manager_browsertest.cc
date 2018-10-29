// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/system_web_app_manager.h"

#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/web_applications/bookmark_apps/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

constexpr char kSystemAppManifestText[] =
    R"({
      "name": "Test System App",
      "display": "standalone",
      "icons": [
        {
          "src": "icon-256.png",
          "sizes": "256x256",
          "type": "image/png"
        }
      ],
      "start_url": "/",
      "theme_color": "#00FF00"
    })";

// WebUIController that serves a System PWA.
class TestWebUIController : public content::WebUIController {
 public:
  explicit TestWebUIController(content::WebUI* web_ui)
      : WebUIController(web_ui) {
    content::WebUIDataSource* data_source =
        content::WebUIDataSource::Create("test-system-app");
    data_source->AddResourcePath("icon-256.png", IDR_PRODUCT_LOGO_256);
    data_source->AddResourcePath("pwa.html", IDR_PWA_HTML);
    data_source->SetRequestFilter(base::BindRepeating(
        [](const std::string& id,
           const content::WebUIDataSource::GotDataCallback& callback) {
          scoped_refptr<base::RefCountedString> ref_contents(
              new base::RefCountedString);
          if (id != "manifest.json")
            return false;

          ref_contents->data() = kSystemAppManifestText;

          callback.Run(ref_contents);
          return true;
        }));
    content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                  data_source);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIController);
};

// WebUIControllerFactory that serves our TestWebUIController.
class TestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() {}

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) const override {
    return std::make_unique<TestWebUIController>(web_ui);
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) const override {
    return reinterpret_cast<content::WebUI::TypeID>(1);
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) const override {
    return true;
  }
  bool UseWebUIBindingsForURL(content::BrowserContext* browser_context,
                              const GURL& url) const override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIControllerFactory);
};

class SystemWebAppManagerIntegrationTest
    : public extensions::ExtensionBrowserTest {
 public:
  SystemWebAppManagerIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAWindowing, features::kSystemWebApps}, {});
    content::WebUIControllerFactory::RegisterFactory(&factory_);
  }
  ~SystemWebAppManagerIntegrationTest() override {
    content::WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    // Reset WebAppProvider so that its SystemWebAppManager doesn't interfere
    // with tests.
    WebAppProvider::Get(profile())->Reset();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestWebUIControllerFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManagerIntegrationTest);
};

// Test that System Apps install correctly with a manifest.
IN_PROC_BROWSER_TEST_F(SystemWebAppManagerIntegrationTest, WithManifest) {
  std::vector<GURL> system_apps;
  system_apps.emplace_back(GURL("chrome://test-system-app/pwa.html"));
  extensions::PendingBookmarkAppManager pending_app_manager(profile());
  TestSystemWebAppManager system_web_app_manager(
      profile(), &pending_app_manager, std::move(system_apps));
  const extensions::Extension* app =
      extensions::TestExtensionRegistryObserver(
          extensions::ExtensionRegistry::Get(profile()))
          .WaitForExtensionInstalled();
  EXPECT_EQ("Test System App", app->name());
  EXPECT_EQ(SkColorSetRGB(0, 0xFF, 0),
            extensions::AppThemeColorInfo::GetThemeColor(app));
  EXPECT_TRUE(app->from_bookmark());
  EXPECT_EQ(extensions::Manifest::EXTERNAL_COMPONENT, app->location());

  // The app should be a PWA.
  EXPECT_EQ(extensions::util::GetInstalledPwaForUrl(
                profile(), GURL("chrome://test-system-app/")),
            app);
}

}  // namespace web_app
