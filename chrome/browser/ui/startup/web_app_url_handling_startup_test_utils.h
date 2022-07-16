// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_WEB_APP_URL_HANDLING_STARTUP_TEST_UTILS_H_
#define CHROME_BROWSER_UI_STARTUP_WEB_APP_URL_HANDLING_STARTUP_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"

namespace base {
class CommandLine;
}

namespace views {
class Widget;
}

class KeyedService;
class Profile;

namespace web_app {

class StartupBrowserWebAppUrlHandlingTest : public InProcessBrowserTest {
 protected:
  StartupBrowserWebAppUrlHandlingTest();
  ~StartupBrowserWebAppUrlHandlingTest() override;

  AppId InstallWebAppWithUrlHandlers(
      const std::vector<apps::UrlHandlerInfo>& url_handlers);

  base::CommandLine SetUpCommandLineWithUrl(const std::string& url);

  void Start(const base::CommandLine& command_line);

  void SetUpCommandlineAndStart(const std::string& url);

  void AutoCloseDialog(views::Widget* widget);

  std::string start_url;
  std::u16string app_name;

 private:
  static std::unique_ptr<KeyedService> CreateFakeWebAppProvider(
      Profile* profile);

  FakeWebAppProviderCreator fake_web_app_provider_creator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_STARTUP_WEB_APP_URL_HANDLING_STARTUP_TEST_UTILS_H_
