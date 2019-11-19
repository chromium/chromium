// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"

class Browser;
class KeyedService;

namespace extensions {
class Extension;
}

namespace web_app {

class TestSystemWebAppManager;
class TestWebUIControllerFactory;
enum class SystemAppType;

class SystemWebAppManagerBrowserTest : public InProcessBrowserTest {
 public:
  // Performs common initialization for testing SystemWebAppManager features.
  // If true, |install_mock| installs a WebUIController that serves a mock
  // System PWA, and ensures the WebAppProvider associated with the startup
  // profile is a TestWebAppProviderCreator.
  explicit SystemWebAppManagerBrowserTest(bool install_mock = true);

  ~SystemWebAppManagerBrowserTest() override;

  // Gets the Extension* from the HostedAppBrowserController associated with
  // |browser|.
  static const extensions::Extension* GetExtensionForAppBrowser(
      Browser* browser);

  // Returns the SystemWebAppManager for browser()->profile(). This will be a
  // TestSystemWebAppManager if initialized with |install_mock| true.
  SystemWebAppManager& GetManager();

  Browser* WaitForSystemAppInstallAndLaunch(SystemAppType system_app_type);

 private:
  std::unique_ptr<KeyedService> CreateWebAppProvider(Profile* profile);

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestWebUIControllerFactory> factory_;
  std::unique_ptr<TestWebAppProviderCreator> test_web_app_provider_creator_;
  TestSystemWebAppManager* test_system_web_app_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManagerBrowserTest);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_SYSTEM_WEB_APP_MANAGER_BROWSERTEST_H_
