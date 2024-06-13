// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_install_dialog_controller.h"

#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/test/run_until.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/lacros/lacros_service.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "content/public/test/browser_test_utils.h"
#endif

namespace web_app {

using SubAppsInstallDialogControllerBrowserTest =
    IsolatedWebAppBrowserTestHarness;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool IsGetAllOpenTabURLsSupported() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::TestController>()) {
    return false;
  }

  return lacros_service
             ->GetInterfaceVersion<crosapi::mojom::TestController>() >=
         static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                              kGetAllOpenTabURLsMinVersion);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(SubAppsInstallDialogControllerBrowserTest,
                       ManageLinkOpensSettingsPage) {
  test::WaitUntilWebAppProviderAndSubsystemsReady(&provider());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::TestControllerAsyncWaiter waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  ash::SystemWebAppManager::Get(browser()->profile())
      ->InstallSystemAppsForTesting();
#endif

  std::unique_ptr<net::EmbeddedTestServer> iwa_dev_server =
      CreateAndStartDevServer(
          FILE_PATH_LITERAL("web_apps/subapps_isolated_app"));

  IsolatedWebAppUrlInfo parent_app = web_app::InstallDevModeProxyIsolatedWebApp(
      browser()->profile(), iwa_dev_server->GetOrigin());
  const webapps::AppId parent_app_id = parent_app.app_id();

  auto controller = std::make_unique<SubAppsInstallDialogController>();
  controller->Init(base::DoNothing(), {},
                   provider().registrar_unsafe().GetAppShortName(parent_app_id),
                   parent_app_id, profile(),
                   browser()
                       ->tab_strip_model()
                       ->GetActiveWebContents()
                       ->GetTopLevelNativeWindow());
  views::Widget* widget = controller->GetWidgetForTesting();
  views::View* manage_permissions_link =
      widget->GetContentsView()->GetViewByID(base::to_underlying(
          SubAppsInstallDialogController::SubAppsInstallDialogViewID::
              MANAGE_PERMISSIONS_LINK));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  content::WebContentsAddedObserver new_tab_observer;
#endif

  static_cast<views::StyledLabel*>(manage_permissions_link)
      ->ClickFirstLinkForTesting();

  std::string app_settings_url = "chrome://os-settings/app-management";
  GURL parent_app_settings_url(app_settings_url +
                               "/detail?id=" + parent_app_id);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (IsGetAllOpenTabURLsSupported()) {
    std::vector<GURL> open_urls;
    EXPECT_TRUE(base::test::RunUntil([&]() -> bool {
      waiter.GetAllOpenTabURLs(&open_urls);
      return base::Contains(open_urls, parent_app_settings_url);
    })) << "Timeout waiting for settings page at "
        << parent_app_settings_url << " to open in Ash. Open Ash windows:\n"
        << base::JoinString(base::ToVector(open_urls, &GURL::spec), "\n");
  } else {
    bool open;
    waiter.CheckAtLeastOneAshBrowserWindowOpen(&open);
    EXPECT_TRUE(open) << "Timout waiting for settings page to open.";
  }
  CloseAllAshBrowserWindows();

#elif BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(base::test::RunUntil([&]() -> bool {
    return parent_app_settings_url ==
           new_tab_observer.GetWebContents()->GetLastCommittedURL();
  }));
#endif
}

}  // namespace web_app
