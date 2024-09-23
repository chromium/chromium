// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/installable/ml_promotion_browsertest_base.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace webapps {

MLPromotionBrowserTestBase::MLPromotionBrowserTestBase()
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
#if !BUILDFLAG(IS_ANDROID)
  os_hooks_suppress_.emplace();
#endif  // !BUILDFLAG(IS_ANDROID)
}

MLPromotionBrowserTestBase::~MLPromotionBrowserTestBase() = default;

void MLPromotionBrowserTestBase::SetUp() {
// TODO(b/287255120) : Build functionalities for Android.
#if !BUILDFLAG(IS_ANDROID)
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  webapps::TestAppBannerManagerDesktop::SetUp();
#endif  // !BUIDLFLAG(IS_ANDROID)

  PlatformBrowserTest::SetUp();
}

void MLPromotionBrowserTestBase::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  // By default, all SSL cert checks are valid. Can be overridden in tests.
  cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(https_server()->Start());

  // Create default responses for the ML system.
  WebappsClient* client = WebappsClient::Get();
  service_override_ = client->OverrideSegmentationServiceForTesting(
      std::make_unique<
          segmentation_platform::MockSegmentationPlatformService>());
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels.push_back("DontShow");
  ON_CALL(*GetMockSegmentation(),
          GetClassificationResult(
              segmentation_platform::kWebAppInstallationPromoKey, testing::_,
              testing::_, base::test::IsNotNullCallback()))
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<3>(result));
  ON_CALL(*GetMockSegmentation(),
          CollectTrainingData(
              segmentation_platform::proto::SegmentId::
                  OPTIMIZATION_TARGET_WEB_APP_INSTALLATION_PROMO,
              testing::_, testing::_, base::test::IsNotNullCallback()))
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<3>(true));

// TODO(b/287255120) : Build functionalities for Android.
#if !BUILDFLAG(IS_ANDROID)
  web_app::test::WaitUntilReady(
      web_app::WebAppProvider::GetForTest(browser()->profile()));
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool MLPromotionBrowserTestBase::InstallAppForCurrentWebContents(
    bool install_locally) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/287255120) : Build functionalities for Android.
  return false;
#else
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(browser()->profile());
  base::test::TestFuture<const webapps::AppId&, InstallResultCode>
      install_future;

  provider->scheduler().FetchManifestAndInstall(
      WebappInstallSource::OMNIBOX_INSTALL_ICON, web_contents()->GetWeakPtr(),
      base::BindOnce(web_app::test::TestAcceptDialogCallback),
      install_future.GetCallback(),
      web_app::FallbackBehavior::kUseFallbackInfoWhenNotInstallable);

  bool success = install_future.Wait();
  if (!success) {
    return success;
  }

  const webapps::AppId& app_id = install_future.Get<webapps::AppId>();
  if (!install_locally) {
    provider->sync_bridge_unsafe().SetAppNotLocallyInstalledForTesting(app_id);
  }
  return success;
#endif  // BUILDFLAG(IS_ANDROID)
}

bool MLPromotionBrowserTestBase::InstallAppFromUserInitiation(
    bool accept_install,
    std::string dialog_name) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/287255120) : Build functionalities for Android.
  return false;
#else
  base::test::TestFuture<const webapps::AppId&, InstallResultCode>
      install_future;
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       dialog_name);
  web_app::CreateWebAppFromManifest(
      web_contents(),
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      install_future.GetCallback(), web_app::PwaInProductHelpState::kNotShown);
  views::Widget* widget = waiter.WaitIfNeededAndGet();
  views::test::WidgetDestroyedWaiter destroyed(widget);
  if (accept_install) {
    views::test::AcceptDialog(widget);
  } else {
    views::test::CancelDialog(widget);
  }
  destroyed.Wait();
  if (!install_future.Wait()) {
    return false;
  }
  if (accept_install) {
    return install_future.Get<InstallResultCode>() ==
           InstallResultCode::kSuccessNewInstall;
  } else {
    return install_future.Get<InstallResultCode>() ==
           InstallResultCode::kUserInstallDeclined;
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

bool MLPromotionBrowserTestBase::NavigateAndAwaitInstallabilityCheck(
    const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/287255120) : Build functionalities for Android.
  return false;
#else
  auto* manager = TestAppBannerManagerDesktop::FromWebContents(web_contents());
  web_app::NavigateViaLinkClickToURLAndWait(browser(), url);
  return manager->WaitForInstallableCheck();
#endif  // BUILDFLAG(IS_ANDROID)
}

segmentation_platform::MockSegmentationPlatformService*
MLPromotionBrowserTestBase::GetMockSegmentation() {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/287255120) : Build functionalities for Android.
  return nullptr;
#else
  // Since we've mocked out the segmentation platform, no browser context is
  // needed.
  return static_cast<segmentation_platform::MockSegmentationPlatformService*>(
      WebappsClient::Get()->GetSegmentationPlatformService(
          /*browsing_context=*/nullptr));
#endif  // BUILDFLAG(IS_ANDROID)
}

net::EmbeddedTestServer* MLPromotionBrowserTestBase::https_server() {
  return &https_server_;
}

content::WebContents* MLPromotionBrowserTestBase::web_contents() {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/287255120) : Build functionalities for Android.
  return nullptr;
#else
  return browser()->tab_strip_model()->GetActiveWebContents();
#endif  // BUILDFLAG(IS_ANDROID)
}

Profile* MLPromotionBrowserTestBase::profile() {
#if BUILDFLAG(IS_ANDROID)
  // TODO(b/287255120) : Build functionalities for Android.
  return nullptr;
#else
  return browser()->profile();
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace webapps
