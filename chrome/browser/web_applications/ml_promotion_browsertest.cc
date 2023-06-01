// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/metrics/site_quality_metrics_task.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace web_app {
namespace {

// TODO(b/279521783): Visit an installable page & verify UKM metrics are
// recorded.

using MLInstallabilityPromoter = webapps::MLInstallabilityPromoter;
using SiteInstallMetrics = webapps::SiteInstallMetrics;
using SiteQualityMetrics = webapps::SiteQualityMetrics;

class ServiceWorkerLoadAwaiter : public content::ServiceWorkerContextObserver {
 public:
  ServiceWorkerLoadAwaiter(content::WebContents* web_contents, const GURL& url)
      : site_url_(url) {
    CHECK(web_contents);
    context_ = web_contents->GetPrimaryMainFrame()
                   ->GetStoragePartition()
                   ->GetServiceWorkerContext();
    context_->AddObserver(this);
  }

  ~ServiceWorkerLoadAwaiter() override {
    if (context_) {
      context_->RemoveObserver(this);
    }
  }

  bool AwaitRegistration() {
    run_loop_.Run();
    return service_worker_reg_complete_;
  }

 private:
  void OnRegistrationCompleted(const GURL& pattern) override {
    if (content::ServiceWorkerContext::ScopeMatches(pattern, site_url_)) {
      service_worker_reg_complete_ = true;
      run_loop_.Quit();
    }
  }

  void OnDestruct(content::ServiceWorkerContext* context) override {
    if (context_) {
      context_->RemoveObserver(this);
      context_ = nullptr;
    }
  }

  const GURL site_url_;
  raw_ptr<content::ServiceWorkerContext> context_;
  bool service_worker_reg_complete_ = false;
  base::RunLoop run_loop_;
};

class MLPromotionBrowsertest : public WebAppControllerBrowserTest {
 public:
  MLPromotionBrowsertest() {
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    scoped_feature_list_.InitAndEnableFeature(
        webapps::features::kWebAppsMlUkmCollection);
  }
  ~MLPromotionBrowsertest() override = default;

  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();
    ml_promoter()->SetTaskRunnerForTesting(task_runner_);
  }

 protected:
  GURL GetUrlWithNoManifest() {
    return https_server()->GetURL("/banners/no_manifest_test_page.html");
  }

  GURL GetUrlWithNoSWNoFetchHandler() {
    return https_server()->GetURL("/banners/manifest_no_service_worker.html");
  }

  GURL GetUrlWithSWEmptyFetchHandler() {
    return https_server()->GetURL(
        "/banners/manifest_test_page_empty_fetch_handler.html");
  }

  GURL GetUrlWithSwNoFetchHandler() {
    return https_server()->GetURL(
        "/banners/no_sw_fetch_handler_test_page.html");
  }

  MLInstallabilityPromoter* ml_promoter() {
    return MLInstallabilityPromoter::FromWebContents(web_contents());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Wait for service workers to be registered and the MLInstallabilityPromoter
  // to finish
  void AwaitServiceWorkerRegistrationAndPendingDelayedTask(const GURL& url) {
    base::test::TestFuture<void> wait_site_load_future;
    ml_promoter()->SetAwaitSiteResourcesLoadCallbackForTesting(
        wait_site_load_future.GetCallback());
    ServiceWorkerLoadAwaiter service_worker_loader(web_contents(), url);
    EXPECT_TRUE(service_worker_loader.AwaitRegistration());
    EXPECT_TRUE(wait_site_load_future.Wait());
  }

  void NavigateAndAwaitMetricsCollectionPending(const GURL& url) {
    base::test::TestFuture<void> delayed_task_future;
    ml_promoter()->SetAwaitSiteResourcesLoadCallbackForTesting(
        delayed_task_future.GetCallback());
    NavigateAndAwaitInstallabilityCheck(browser(), url);
    EXPECT_TRUE(delayed_task_future.Wait());
  }

  bool InstallApp(bool install_locally = true) {
    WebAppProvider* provider = WebAppProvider::GetForTest(browser()->profile());
    base::test::TestFuture<const AppId&, webapps::InstallResultCode>
        install_future;

    provider->scheduler().FetchManifestAndInstall(
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        web_contents()->GetWeakPtr(),
        /*bypass_service_worker_check=*/false,
        base::BindOnce(test::TestAcceptDialogCallback),
        install_future.GetCallback(), /*use_fallback=*/false);

    bool success = install_future.Wait();
    if (!success) {
      return success;
    }

    const AppId& app_id = install_future.Get<AppId>();
    provider->sync_bridge_unsafe().SetAppIsLocallyInstalledForTesting(
        app_id, /*is_locally_installed=*/install_locally);
    return success;
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Manifest Data Fetching tests.
IN_PROC_BROWSER_TEST_F(MLPromotionBrowsertest, FilledManifestRead) {
  NavigateAndAwaitMetricsCollectionPending(GetInstallableAppURL());
  task_runner_->RunPendingTasks();

  const blink::mojom::ManifestPtr& obtained_manifest =
      ml_promoter()->GetManifestForTesting();
  EXPECT_FALSE(blink::IsEmptyManifest(obtained_manifest));

  EXPECT_EQ(u"Manifest test app", obtained_manifest->name);
  EXPECT_EQ(https_server()->GetURL("/banners/manifest_test_page.html"),
            obtained_manifest->start_url);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowsertest, EmptyManifestRead) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithNoManifest());
  task_runner_->RunPendingTasks();

  const blink::mojom::ManifestPtr& obtained_manifest =
      ml_promoter()->GetManifestForTesting();
  EXPECT_TRUE(blink::IsEmptyManifest(obtained_manifest));
}

// TODO(b/284157768): Fix flakiness for addManifestLinkTag calls.
IN_PROC_BROWSER_TEST_F(MLPromotionBrowsertest,
                       DISABLED_ManifestChangesNewManifestLoads) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithNoManifest());
  EXPECT_TRUE(content::ExecJs(
      web_contents(),
      "addManifestLinkTag('/banners/manifest_for_no_manifest_page.json')"));

  // Ensure that the manifest URL change is propagated to the metrics collector.
  base::RunLoop().RunUntilIdle();
  // This runs the delayed tasks used to wait for changes to the web contents.
  task_runner_->RunPendingTasks();

  // By the time the data collection callback is run, the manifest should have
  // been updated and properly read.
  const blink::mojom::ManifestPtr& obtained_manifest =
      ml_promoter()->GetManifestForTesting();
  EXPECT_FALSE(blink::IsEmptyManifest(obtained_manifest));

  EXPECT_EQ(u"Manifest test app", obtained_manifest->name);
  EXPECT_EQ(https_server()->GetURL("/banners/no_manifest_test_page.html"),
            obtained_manifest->start_url);
}

// SiteInstallMetrics tests.
IN_PROC_BROWSER_TEST_F(MLPromotionBrowsertest, FullyInstalledAppMeasurement) {
  NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL());
  EXPECT_TRUE(InstallApp());

  NavigateAndAwaitInstallabilityCheck(browser(), GetUrlWithNoManifest());

  // A re-navigation should retrigger the ML pipeline.
  NavigateAndAwaitMetricsCollectionPending(GetInstallableAppURL());
  task_runner_->RunPendingTasks();

  const SiteInstallMetrics& install_metrics =
      ml_promoter()->GetSiteInstallMetricsForTesting();
  EXPECT_TRUE(install_metrics.is_fully_installed);
  EXPECT_FALSE(install_metrics.is_partially_installed);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowsertest,
                       PartiallyInstalledAppMeasurement) {
  NavigateAndAwaitInstallabilityCheck(browser(), GetInstallableAppURL());
  EXPECT_TRUE(InstallApp(/*install_locally=*/false));

  NavigateAndAwaitInstallabilityCheck(browser(), GetUrlWithNoManifest());
  // A re-navigation should retrigger the ML pipeline.
  NavigateAndAwaitMetricsCollectionPending(GetInstallableAppURL());
  task_runner_->RunPendingTasks();
  // EXPECT_TRUE(data_collection_future.Wait());

  const SiteInstallMetrics& install_metrics =
      ml_promoter()->GetSiteInstallMetricsForTesting();
  EXPECT_FALSE(install_metrics.is_fully_installed);
  EXPECT_TRUE(install_metrics.is_partially_installed);
}

// SiteQualityMetrics tests.
// TODO(crbug.com/1450421): Disabled for being flaky.
IN_PROC_BROWSER_TEST_F(MLPromotionBrowsertest,
                       DISABLED_SiteQualityMetrics_ServiceWorker_FetchHandler) {
  NavigateAndAwaitMetricsCollectionPending(GetInstallableAppURL());
  AwaitServiceWorkerRegistrationAndPendingDelayedTask(GetInstallableAppURL());
  task_runner_->RunPendingTasks();

  const SiteQualityMetrics& quality_metrics =
      ml_promoter()->GetSiteQualityMetricsForTesting();

  EXPECT_TRUE(quality_metrics.has_service_worker);
  EXPECT_TRUE(quality_metrics.has_fetch_handler);

  // TODO(b/284157768): The quota service does not accurately collect service
  // worker script data, uncomment once that is fixed.
  // EXPECT_GT(quality_metrics.service_worker_script_size, 0);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowsertest,
                       SiteQualityMetrics_NoServiceWorker_NoFetchHandler) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithNoSWNoFetchHandler());
  task_runner_->RunPendingTasks();

  const SiteQualityMetrics& quality_metrics =
      ml_promoter()->GetSiteQualityMetricsForTesting();
  EXPECT_FALSE(quality_metrics.has_service_worker);
  EXPECT_FALSE(quality_metrics.has_fetch_handler);
  EXPECT_EQ(quality_metrics.service_worker_script_size, 0);
}

// TODO(crbug.com/1450421): Disabled for being flaky.
IN_PROC_BROWSER_TEST_F(
    MLPromotionBrowsertest,
    DISABLED_SiteQualityMetrics_ServiceWorker_EmptyFetchHandler) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithSWEmptyFetchHandler());
  AwaitServiceWorkerRegistrationAndPendingDelayedTask(
      GetUrlWithSWEmptyFetchHandler());
  task_runner_->RunPendingTasks();

  const SiteQualityMetrics& quality_metrics =
      ml_promoter()->GetSiteQualityMetricsForTesting();

  // An empty fetch handler is also treated as an existence of a fetch handler.
  EXPECT_TRUE(quality_metrics.has_service_worker);
  EXPECT_TRUE(quality_metrics.has_fetch_handler);
}

// TODO(crbug.com/1450421): Disabled for being flaky.
IN_PROC_BROWSER_TEST_F(
    MLPromotionBrowsertest,
    DISABLED_SiteQualityMetrics_ServiceWorker_NoFetchHandler) {
  NavigateAndAwaitMetricsCollectionPending(GetUrlWithSwNoFetchHandler());
  AwaitServiceWorkerRegistrationAndPendingDelayedTask(
      GetUrlWithSwNoFetchHandler());
  task_runner_->RunPendingTasks();

  const SiteQualityMetrics& quality_metrics =
      ml_promoter()->GetSiteQualityMetricsForTesting();
  EXPECT_TRUE(quality_metrics.has_service_worker);
  EXPECT_FALSE(quality_metrics.has_fetch_handler);
}

IN_PROC_BROWSER_TEST_F(MLPromotionBrowsertest, PageLoadsWithOnly1Favicon) {
  NavigateAndAwaitMetricsCollectionPending(GetInstallableAppURL());
  task_runner_->RunPendingTasks();

  const SiteQualityMetrics& quality_metrics =
      ml_promoter()->GetSiteQualityMetricsForTesting();
  EXPECT_EQ(1u, quality_metrics.favicons_count);
}

// TODO(b/284157768): Add ServiceWorker cache tests for QuotaManager and
// FaviconURL update tests.

}  // namespace
}  // namespace web_app
