// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/test/web_app_install_observer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace web_app {

class WebAppProfileDeletionBrowserTest : public WebAppControllerBrowserTest {
 public:
  AppRegistrar& registrar() {
    auto* provider = WebAppProviderBase::GetProviderBase(profile());
    CHECK(provider);
    return provider->registrar();
  }

  void ScheduleCurrentProfileForDeletion() {
    g_browser_process->profile_manager()->ScheduleProfileForDeletion(
        profile()->GetPath(), base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(WebAppProfileDeletionBrowserTest,
                       AppRegistrarNotifiesProfileDeletion) {
  GURL app_url(GetInstallableAppURL());
  const AppId app_id = InstallPWA(app_url);

  base::RunLoop run_loop;
  WebAppInstallObserver observer(&registrar());
  observer.SetWebAppProfileWillBeDeletedDelegate(
      base::BindLambdaForTesting([&](const AppId& app_to_be_uninstalled) {
        EXPECT_EQ(app_to_be_uninstalled, app_id);
        EXPECT_TRUE(registrar().IsInstalled(app_id));
        EXPECT_TRUE(registrar().AsWebAppRegistrar()->GetAppById(app_id));

        run_loop.Quit();
      }));

  ScheduleCurrentProfileForDeletion();
  run_loop.Run();

  EXPECT_FALSE(registrar().IsInstalled(app_id));
  EXPECT_FALSE(registrar().AsWebAppRegistrar()->GetAppById(app_id));
}

}  // namespace web_app
