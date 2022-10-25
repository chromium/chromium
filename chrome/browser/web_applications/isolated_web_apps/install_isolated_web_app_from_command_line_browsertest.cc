// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_from_command_line.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/one_shot_event.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace web_app {
namespace {
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsTrue;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;

class InstallIsolatedWebAppFromCommandLineBrowserTest
    : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
    embedded_test_server()->AddDefaultHandlers(
        GetChromeTestDataDir().AppendASCII("web_apps/simple_isolated_app"));
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DCHECK(command_line != nullptr);

    command_line->AppendSwitchASCII("install-isolated-web-app-from-url",
                                    GetAppUrl().spec());
  }

  GURL GetAppUrl() const { return embedded_test_server()->base_url(); }

  WebAppRegistrar& GetWebAppRegistrar() {
    auto* provider = WebAppProvider::GetForTest(browser()->profile());
    DCHECK(provider != nullptr);
    return provider->registrar();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InstallIsolatedWebAppFromCommandLineBrowserTest,
                       AppFromCommandLineIsInstalled) {
  WebAppTestInstallObserver observer(browser()->profile());
  AppId id = observer.BeginListeningAndWait();

  ASSERT_THAT(GetWebAppRegistrar().IsInstalled(id), IsTrue());

  EXPECT_THAT(GetWebAppRegistrar().GetAppById(id),
              Pointee(Property(&WebApp::isolation_data, Optional(_))));
}

}  // namespace
}  // namespace web_app
