// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/web_app_url_handling_startup_test_utils.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/url_handler_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "third_party/blink/public/common/features.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace web_app {

StartupBrowserWebAppUrlHandlingTest::StartupBrowserWebAppUrlHandlingTest()
    : start_url("https://test.com"),
      app_name(u"Test App"),
      fake_web_app_provider_creator_(base::BindRepeating(
          &StartupBrowserWebAppUrlHandlingTest::CreateFakeWebAppProvider)) {
  scoped_feature_list_.InitAndEnableFeature(
      blink::features::kWebAppEnableUrlHandlers);
}

StartupBrowserWebAppUrlHandlingTest::~StartupBrowserWebAppUrlHandlingTest() =
    default;

AppId StartupBrowserWebAppUrlHandlingTest::InstallWebAppWithUrlHandlers(
    const std::vector<apps::UrlHandlerInfo>& url_handlers) {
  return test::InstallWebAppWithUrlHandlers(
      browser()->profile(), GURL(start_url), app_name, url_handlers);
}

base::CommandLine StartupBrowserWebAppUrlHandlingTest::SetUpCommandLineWithUrl(
    const std::string& url) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendArg(url);
  return command_line;
}

void StartupBrowserWebAppUrlHandlingTest::Start(
    const base::CommandLine& command_line) {
  std::vector<Profile*> last_opened_profiles;
  StartupBrowserCreator browser_creator;
  browser_creator.Start(command_line,
                        g_browser_process->profile_manager()->user_data_dir(),
                        browser()->profile(), last_opened_profiles);
}

void StartupBrowserWebAppUrlHandlingTest::SetUpCommandlineAndStart(
    const std::string& url) {
  Start(SetUpCommandLineWithUrl(url));
}

void StartupBrowserWebAppUrlHandlingTest::AutoCloseDialog(
    views::Widget* widget) {
  // Call CancelDialog to close the dialog, but the actual behavior will be
  // determined by the ScopedTestDialogAutoConfirm configs.
  views::test::CancelDialog(widget);
}

// static
std::unique_ptr<KeyedService>
StartupBrowserWebAppUrlHandlingTest::CreateFakeWebAppProvider(
    Profile* profile) {
  auto provider = std::make_unique<FakeWebAppProvider>(profile);
  provider->Start();
  auto association_manager =
      std::make_unique<FakeWebAppOriginAssociationManager>();
  association_manager->set_pass_through(true);
  auto& url_handler_manager =
      provider->os_integration_manager().url_handler_manager_for_testing();
  url_handler_manager.SetAssociationManagerForTesting(
      std::move(association_manager));
  return provider;
}

}  // namespace web_app
