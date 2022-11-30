// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_app_from_command_line.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace web_app {
namespace {
using ::testing::IsTrue;

class InstallIsolatedAppFromCommandLineBrowserTest
    : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(
        GetChromeTestDataDir().AppendASCII("web_apps/simple_isolated_app"));
    ASSERT_TRUE(embedded_test_server()->Start());

    SetNextInstallationDoneCallbackForTesting(
        base::BindLambdaForTesting([&]() { is_installation_done_.Signal(); }));

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DCHECK(command_line != nullptr);

    command_line->AppendSwitchASCII("install-isolated-web-app-from-url",
                                    GetAppUrl().spec());
  }

  GURL GetAppUrl() const { return embedded_test_server()->base_url(); }

  void WaitForInstallation() {
    base::RunLoop loop;
    is_installation_done_.Post(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  WebAppRegistrar& GetWebAppRegistrar() {
    auto* provider = WebAppProvider::GetForTest(browser()->profile());
    DCHECK(provider != nullptr);
    return provider->registrar();
  }

 private:
  base::OneShotEvent is_installation_done_;
};

IN_PROC_BROWSER_TEST_F(InstallIsolatedAppFromCommandLineBrowserTest,
                       AppFromCommandLineIsInstalled) {
  WaitForInstallation();

  const AppId app_id = GenerateAppId("", GetAppUrl());

  ASSERT_THAT(GetWebAppRegistrar().IsInstalled(app_id), IsTrue());

  const WebApp* web_app = GetWebAppRegistrar().GetAppById(app_id);
  EXPECT_THAT(web_app->isolation_data().has_value(), IsTrue());
}
}  // namespace
}  // namespace web_app
