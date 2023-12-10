// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/remove_isolated_web_app_data.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/canonical_cookie.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::Eq;

namespace web_app {
namespace {

struct TestCookie {
  base::Time time;
  std::string data;
};

}  // namespace

// Evaluates to true if the test value is within 5% of the given value.
MATCHER_P(IsApproximately, approximate_value, "") {
  return arg > (approximate_value * 0.95) && arg < (approximate_value * 1.05);
}

class IsolatedWebAppBrowsingDataTest : public IsolatedWebAppBrowserTestHarness {
 protected:
  IsolatedWebAppUrlInfo InstallIsolatedWebApp() {
    server_ =
        CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
    web_app::IsolatedWebAppUrlInfo url_info =
        InstallDevModeProxyIsolatedWebApp(server_->GetOrigin());
    return url_info;
  }

  net::EmbeddedTestServer* dev_server() { return server_.get(); }

  WebAppProvider& web_app_provider() {
    return CHECK_DEREF(WebAppProvider::GetForTest(profile()));
  }

  int64_t GetIwaUsage(const IsolatedWebAppUrlInfo& url_info) {
    base::test::TestFuture<base::flat_map<url::Origin, int64_t>> future;
    web_app_provider().scheduler().GetIsolatedWebAppBrowsingData(
        future.GetCallback());
    base::flat_map<url::Origin, int64_t> result = future.Get();
    return result.contains(url_info.origin()) ? result.at(url_info.origin())
                                              : 0;
  }

  void AddLocalStorageIfMissing(const content::ToRenderFrameHost& target) {
    EXPECT_TRUE(
        ExecJs(target, "localStorage.setItem('test', '!'.repeat(1000))"));

    base::test::TestFuture<void> test_future;
    target.render_frame_host()
        ->GetStoragePartition()
        ->GetLocalStorageControl()
        ->Flush(test_future.GetCallback());
    EXPECT_TRUE(test_future.Wait());
  }

  void SetLocalStorageValue(const content::ToRenderFrameHost& target,
                            const base::StringPiece& key,
                            const base::StringPiece& value) {
    EXPECT_TRUE(
        ExecJs(target,
               content::JsReplace("localStorage.setItem($1, $2)", key, value)));
  }

  std::string GetLocalStorageValue(const content::ToRenderFrameHost& target,
                                   const base::StringPiece& key) {
    return EvalJs(target, content::JsReplace("localStorage.getItem($1)", key))
        .ExtractString();
  }

  int GetLocalStorageCount(const content::ToRenderFrameHost& target) {
    return EvalJs(target, "localStorage.length").ExtractInt();
  }

  [[nodiscard]] bool CreateControlledFrame(content::WebContents* web_contents,
                                           const GURL& src,
                                           const std::string& partition) {
    static std::string kCreateControlledFrame = R"(
      (async function() {
        const controlledframe = document.createElement('controlledframe');
        controlledframe.setAttribute('src', $1);
        controlledframe.setAttribute('partition', $2);
        await new Promise((resolve, reject) => {
          controlledframe.addEventListener('loadcommit', resolve);
          controlledframe.addEventListener('loadabort', reject);
          document.body.appendChild(controlledframe);
        });
      })();
    )";
    return ExecJs(web_contents,
                  content::JsReplace(kCreateControlledFrame, src, partition));
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> server_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowsingDataTest,
                       ControlledFrameUsageIsCounted) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  Browser* browser = LaunchWebAppBrowserAndWait(url_info.app_id());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  EXPECT_THAT(GetIwaUsage(url_info), Eq(0));

  // Add some usage to the IWA and make sure it's counted.
  AddLocalStorageIfMissing(web_contents);
  EXPECT_THAT(GetIwaUsage(url_info), IsApproximately(1000));

  // Create a persisted <controlledframe>, add some usage to it.
  ASSERT_TRUE(CreateControlledFrame(web_contents,
                                    dev_server()->GetURL("/empty_title.html"),
                                    "persist:partition_name"));
  ASSERT_EQ(1UL, web_contents->GetInnerWebContents().size());
  AddLocalStorageIfMissing(web_contents->GetInnerWebContents()[0]);
  EXPECT_THAT(GetIwaUsage(url_info), IsApproximately(2000));

  // Create another persisted <controlledframe> with a different partition name.
  ASSERT_TRUE(CreateControlledFrame(web_contents,
                                    dev_server()->GetURL("/empty_title.html"),
                                    "persist:partition_name_2"));
  ASSERT_EQ(2UL, web_contents->GetInnerWebContents().size());
  AddLocalStorageIfMissing(web_contents->GetInnerWebContents()[0]);
  AddLocalStorageIfMissing(web_contents->GetInnerWebContents()[1]);
  EXPECT_THAT(GetIwaUsage(url_info), IsApproximately(3000));

  // Create an in-memory <controlledframe> that won't count towards IWA usage.
  ASSERT_TRUE(CreateControlledFrame(
      web_contents, dev_server()->GetURL("/empty_title.html"), "unpersisted"));
  ASSERT_EQ(3UL, web_contents->GetInnerWebContents().size());
  AddLocalStorageIfMissing(web_contents->GetInnerWebContents()[0]);
  AddLocalStorageIfMissing(web_contents->GetInnerWebContents()[1]);
  AddLocalStorageIfMissing(web_contents->GetInnerWebContents()[2]);
  EXPECT_THAT(GetIwaUsage(url_info), IsApproximately(3000));
}

class IsolatedWebAppBrowsingDataClearingTest
    : public IsolatedWebAppBrowsingDataTest {
 protected:
  void ClearData(const IsolatedWebAppUrlInfo& url_info) {
    base::RunLoop run_loop;
    auto* browsing_data_remover = profile()->GetBrowsingDataRemover();
    browsing_data_remover->SetWouldCompleteCallbackForTesting(
        base::BindLambdaForTesting([&](base::OnceClosure callback) {
          if (browsing_data_remover->GetPendingTaskCountForTesting() == 1) {
            run_loop.Quit();
          }
          std::move(callback).Run();
        }));

    web_app::RemoveIsolatedWebAppBrowsingData(profile(), url_info.origin(),
                                              base::DoNothing());
    run_loop.Run();

    browsing_data_remover->SetWouldCompleteCallbackForTesting(
        base::DoNothing());
  }

  void ClearTimeRangedData(browsing_data::TimePeriod time_period) {
    base::RunLoop run_loop;

    auto* browsing_data_remover = profile()->GetBrowsingDataRemover();
    browsing_data_remover->SetWouldCompleteCallbackForTesting(
        base::BindLambdaForTesting([&](base::OnceClosure callback) {
          if (browsing_data_remover->GetPendingTaskCountForTesting() == 1) {
            run_loop.Quit();
          }
          std::move(callback).Run();
        }));

    content::RenderFrameHost* rfh = ui_test_utils::NavigateToURL(
        browser(), GURL("chrome://settings/clearBrowserData"));

    for (auto& handler : *rfh->GetWebUI()->GetHandlersForTesting()) {
      handler->AllowJavascriptForTesting();
    }

    base::Value::List data_types;
    // These 3 values reflect 3 checkboxes in the "Basic" tab of
    // chrome://settings/clearBrowserData.
    data_types.Append(browsing_data::prefs::kDeleteBrowsingHistoryBasic);
    data_types.Append(browsing_data::prefs::kDeleteCookiesBasic);
    data_types.Append(browsing_data::prefs::kDeleteCacheBasic);

    base::Value::List list_args;
    list_args.Append("webui_callback_id");
    list_args.Append(std::move(data_types));
    list_args.Append(static_cast<int>(time_period));

    rfh->GetWebUI()->ProcessWebUIMessage(
        rfh->GetLastCommittedURL(), "clearBrowsingData", std::move(list_args));

    run_loop.Run();
  }

  void ClearAllTimeData() {
    ClearTimeRangedData(browsing_data::TimePeriod::ALL_TIME);
  }

  void Uninstall(const IsolatedWebAppUrlInfo& url_info) {
    base::RunLoop run_loop;
    auto* browsing_data_remover = profile()->GetBrowsingDataRemover();
    browsing_data_remover->SetWouldCompleteCallbackForTesting(
        base::BindLambdaForTesting([&](base::OnceClosure callback) {
          if (browsing_data_remover->GetPendingTaskCountForTesting() == 1) {
            run_loop.Quit();
          }
          std::move(callback).Run();
        }));

    base::test::TestFuture<webapps::UninstallResultCode> future;
    auto job = std::make_unique<RemoveWebAppJob>(
        webapps::WebappUninstallSource::kAppsPage, *profile(),
        url_info.app_id());

    provider().scheduler().UninstallWebApp(
        url_info.app_id(), webapps::WebappUninstallSource::kAppsPage,
        future.GetCallback());

    auto code = future.Get();
    ASSERT_TRUE(code == webapps::UninstallResultCode::kSuccess);
    run_loop.Run();
  }

  int64_t GetCacheSize(content::StoragePartition* storage_partition) {
    base::test::TestFuture<bool, int64_t> future;

    storage_partition->GetNetworkContext()->ComputeHttpCacheSize(
        base::Time::Min(), base::Time::Max(),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            future.GetCallback(),
            /* is_upper_limit = */ false,
            /* result_or_error = */ -1));

    std::tuple<bool, int64_t> result = future.Get();

    int64_t cache_size_or_error = std::get<1>(result);
    CHECK(cache_size_or_error >= 0);
    return cache_size_or_error;
  }

  bool SetCookie(
      content::StoragePartition* storage_partition,
      const GURL& url,
      const base::Time time,
      const std::string& cookie_line,
      const absl::optional<net::CookiePartitionKey>& cookie_partition_key) {
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    storage_partition->GetNetworkContext()->GetCookieManager(
        cookie_manager.BindNewPipeAndPassReceiver());

    auto cookie_obj = net::CanonicalCookie::Create(
        url, cookie_line, time, /*server_time=*/absl::nullopt,
        cookie_partition_key);

    base::test::TestFuture<net::CookieAccessResult> future;
    cookie_manager->SetCanonicalCookie(*cookie_obj, url,
                                       net::CookieOptions::MakeAllInclusive(),
                                       future.GetCallback());
    return future.Take().status.IsInclude();
  }

  net::CookieList GetAllCookies(content::StoragePartition* storage_partition) {
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    storage_partition->GetNetworkContext()->GetCookieManager(
        cookie_manager.BindNewPipeAndPassReceiver());
    base::test::TestFuture<const net::CookieList&> future;
    cookie_manager->GetAllCookies(future.GetCallback());
    return future.Take();
  }
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowsingDataClearingTest,
                       LocalStorageCleared) {
  // Install 2 IWAs and add data to each.
  IsolatedWebAppUrlInfo url_info1 = InstallIsolatedWebApp();
  Browser* browser1 = LaunchWebAppBrowserAndWait(url_info1.app_id());
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();

  EXPECT_THAT(GetIwaUsage(url_info1), Eq(0));
  AddLocalStorageIfMissing(web_contents1);
  EXPECT_THAT(GetIwaUsage(url_info1), IsApproximately(1000));

  IsolatedWebAppUrlInfo url_info2 = InstallIsolatedWebApp();
  Browser* browser2 = LaunchWebAppBrowserAndWait(url_info2.app_id());
  content::WebContents* web_contents2 =
      browser2->tab_strip_model()->GetActiveWebContents();

  EXPECT_THAT(GetIwaUsage(url_info2), Eq(0));
  AddLocalStorageIfMissing(web_contents2);
  EXPECT_THAT(GetIwaUsage(url_info2), IsApproximately(1000));

  ASSERT_TRUE(CreateControlledFrame(web_contents2,
                                    dev_server()->GetURL("/empty_title.html"),
                                    "persist:partition_name"));
  ASSERT_EQ(1UL, web_contents2->GetInnerWebContents().size());
  AddLocalStorageIfMissing(web_contents2->GetInnerWebContents()[0]);
  EXPECT_THAT(GetIwaUsage(url_info2), IsApproximately(2000));

  ClearData(url_info2);

  EXPECT_THAT(GetIwaUsage(url_info1), IsApproximately(1000));
  EXPECT_THAT(GetIwaUsage(url_info2), Eq(0));
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowsingDataClearingTest, CacheCleared) {
  auto cache_test_server = std::make_unique<net::EmbeddedTestServer>();
  cache_test_server->AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(cache_test_server->Start());

  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  Browser* browser = LaunchWebAppBrowserAndWait(url_info.app_id());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // Create both a persistent and a non-persistent partitions.
  ASSERT_TRUE(CreateControlledFrame(
      web_contents,
      cache_test_server->GetURL("/page_with_cached_subresource.html"),
      "persist:partition_name_0"));
  ASSERT_TRUE(CreateControlledFrame(
      web_contents,
      cache_test_server->GetURL("/page_with_cached_subresource.html"),
      "partition_name_1"));

  std::vector<content::StoragePartitionConfig> storage_partition_configs{
      url_info.storage_partition_config(profile()),
      url_info.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_0", /*in_memory=*/false),
      url_info.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_1", /*in_memory=*/true)};

  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    ASSERT_GT(GetCacheSize(partition), 0);
  }

  ClearData(url_info);

  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    EXPECT_EQ(GetCacheSize(partition), 0);
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowsingDataClearingTest, CookieCleared) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  Browser* browser = LaunchWebAppBrowserAndWait(url_info.app_id());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // Create both a persistent and a non-persistent partitions.
  ASSERT_TRUE(CreateControlledFrame(web_contents,
                                    dev_server()->GetURL("/empty_title.html"),
                                    "persist:partition_name_0"));
  ASSERT_TRUE(CreateControlledFrame(web_contents,
                                    dev_server()->GetURL("/empty_title.html"),
                                    "partition_name_1"));

  std::vector<content::StoragePartitionConfig> storage_partition_configs{
      url_info.storage_partition_config(profile()),
      url_info.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_0", /*in_memory=*/false),
      url_info.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_1", /*in_memory=*/true)};

  // Set a partitioned and an unpartitioned cookie for each storage partition.
  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    // Unpartitioned Cookie
    ASSERT_TRUE(SetCookie(partition, GURL("http://a.com"), base::Time::Now(),
                          "A=0", absl::nullopt));
    // Partitioned Cookie
    ASSERT_TRUE(SetCookie(
        partition, GURL("https://c.com"), base::Time::Now(),
        "A=0; secure; partitioned",
        net::CookiePartitionKey::FromURLForTesting(GURL("https://d.com"))));
  }

  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    ASSERT_EQ(GetAllCookies(partition).size(), 2UL);
  }

  ClearData(url_info);

  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    EXPECT_EQ(GetAllCookies(partition).size(), 0UL);
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowsingDataClearingTest,
                       DataClearedOnUninstall) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  Browser* browser = LaunchWebAppBrowserAndWait(url_info.app_id());
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // Create both a persistent and a non-persistent partitions.
  ASSERT_TRUE(CreateControlledFrame(web_contents,
                                    dev_server()->GetURL("/empty_title.html"),
                                    "persist:partition_name_0"));
  ASSERT_TRUE(CreateControlledFrame(web_contents,
                                    dev_server()->GetURL("/empty_title.html"),
                                    "partition_name_1"));

  std::vector<content::StoragePartitionConfig> storage_partition_configs{
      url_info.storage_partition_config(profile()),
      url_info.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_0", /*in_memory=*/false),
      url_info.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_1", /*in_memory=*/true)};

  // Set a partitioned and an unpartitioned cookie for each storage partition.
  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    // Unpartitioned Cookie
    ASSERT_TRUE(SetCookie(partition, GURL("http://a.com"), base::Time::Now(),
                          "A=0", absl::nullopt));
    // Partitioned Cookie
    ASSERT_TRUE(SetCookie(
        partition, GURL("https://c.com"), base::Time::Now(),
        "A=0; secure; partitioned",
        net::CookiePartitionKey::FromURLForTesting(GURL("https://d.com"))));
  }

  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    ASSERT_EQ(GetAllCookies(partition).size(), 2UL);
  }

  Uninstall(url_info);

  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    EXPECT_EQ(GetAllCookies(partition).size(), 0UL);
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowsingDataClearingTest,
                       ClearBrowserDataAllTime) {
  auto cache_test_server = std::make_unique<net::EmbeddedTestServer>();
  cache_test_server->AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(cache_test_server->Start());

  // Set up IWA 1.
  IsolatedWebAppUrlInfo url_info1 = InstallIsolatedWebApp();
  Browser* browser1 = LaunchWebAppBrowserAndWait(url_info1.app_id());
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();
  // Create both a persistent and a non-persistent partitions.
  ASSERT_TRUE(CreateControlledFrame(
      web_contents1,
      cache_test_server->GetURL("/page_with_cached_subresource.html"),
      "persist:partition_name_0"));
  ASSERT_TRUE(CreateControlledFrame(
      web_contents1,
      cache_test_server->GetURL("/page_with_cached_subresource.html"),
      "partition_name_1"));

  // Set up IWA 2.
  IsolatedWebAppUrlInfo url_info2 = InstallIsolatedWebApp();
  Browser* browser2 = LaunchWebAppBrowserAndWait(url_info2.app_id());
  content::WebContents* web_contents2 =
      browser2->tab_strip_model()->GetActiveWebContents();
  // Create both a persistent and a non-persistent partitions.
  ASSERT_TRUE(CreateControlledFrame(
      web_contents2,
      cache_test_server->GetURL("/page_with_cached_subresource.html"),
      "persist:partition_name_0"));
  ASSERT_TRUE(CreateControlledFrame(
      web_contents2,
      cache_test_server->GetURL("/page_with_cached_subresource.html"),
      "partition_name_1"));
  // Making IWA 2 a stub.
  {
    ScopedRegistryUpdate update =
        web_app_provider().sync_bridge_unsafe().BeginUpdate();
    update->UpdateApp(url_info2.app_id())->SetIsUninstalling(true);
  }
  ASSERT_TRUE(web_app_provider()
                  .registrar_unsafe()
                  .GetAppById(url_info2.app_id())
                  ->is_uninstalling());

  std::vector<content::StoragePartitionConfig> storage_partition_configs{
      url_info1.storage_partition_config(profile()),
      url_info1.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_0", /*in_memory=*/false),
      url_info1.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_1", /*in_memory=*/true),
      url_info2.storage_partition_config(profile()),
      url_info2.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_0", /*in_memory=*/false),
      url_info2.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_1", /*in_memory=*/true)};

  ASSERT_THAT(GetIwaUsage(url_info1), 0);
  AddLocalStorageIfMissing(web_contents1);
  ASSERT_THAT(GetIwaUsage(url_info1), IsApproximately(1000));
  ASSERT_EQ(web_contents1->GetInnerWebContents().size(), 2UL);
  AddLocalStorageIfMissing(web_contents1->GetInnerWebContents()[0]);
  AddLocalStorageIfMissing(web_contents1->GetInnerWebContents()[1]);
  // 2000 because non-persistent partitions are not counted toward usage.
  ASSERT_THAT(GetIwaUsage(url_info1), IsApproximately(2000));

  // Set a partitioned and an unpartitioned cookie for each storage partition.
  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    // Unpartitioned Cookie
    ASSERT_TRUE(SetCookie(partition, GURL("http://a.com"), base::Time::Now(),
                          "A=0", absl::nullopt));
    // Partitioned Cookie
    ASSERT_TRUE(SetCookie(
        partition, GURL("https://c.com"), base::Time::Now(),
        "A=0; secure; partitioned",
        net::CookiePartitionKey::FromURLForTesting(GURL("https://d.com"))));
  }

  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    // Each partition should have 2 cookies.
    ASSERT_EQ(GetAllCookies(partition).size(), 2UL);
    // Each partition should have cache.
    ASSERT_GT(GetCacheSize(partition), 0);
  }

  ClearAllTimeData();

  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    // Cookies cleared.
    EXPECT_EQ(GetAllCookies(partition).size(), 0UL);
    // Cache cleared.
    EXPECT_EQ(GetCacheSize(partition), 0);
  }
  EXPECT_THAT(GetIwaUsage(url_info1), 0);
  EXPECT_THAT(GetIwaUsage(url_info2), 0);
}

// TODO(crbug.com/1504250): Re-enable this test
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ClearBrowserDataTimeRanged DISABLED_ClearBrowserDataTimeRanged
#else
#define MAYBE_ClearBrowserDataTimeRanged ClearBrowserDataTimeRanged
#endif
IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowsingDataClearingTest,
                       MAYBE_ClearBrowserDataTimeRanged) {
  auto cache_test_server = std::make_unique<net::EmbeddedTestServer>();
  cache_test_server->AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(cache_test_server->Start());

  IsolatedWebAppUrlInfo url_info1 = InstallIsolatedWebApp();
  Browser* browser1 = LaunchWebAppBrowserAndWait(url_info1.app_id());
  content::WebContents* web_contents1 =
      browser1->tab_strip_model()->GetActiveWebContents();
  // Create both a persistent and a non-persistent partitions.
  ASSERT_TRUE(CreateControlledFrame(
      web_contents1,
      cache_test_server->GetURL("/page_with_cached_subresource.html"),
      "persist:partition_name_0"));
  ASSERT_TRUE(CreateControlledFrame(
      web_contents1,
      cache_test_server->GetURL("/page_with_cached_subresource.html"),
      "partition_name_1"));

  std::vector<content::StoragePartitionConfig> storage_partition_configs{
      url_info1.storage_partition_config(profile()),
      url_info1.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_0", /*in_memory=*/false),
      url_info1.GetStoragePartitionConfigForControlledFrame(
          profile(), "partition_name_1", /*in_memory=*/true)};

  // Set cookies for: Now, 3 days ago, 10 days ago.
  const std::vector<TestCookie> cookietest_cases{
      {base::Time::Now() - base::Days(3), "b=1"},
      {base::Time::Now() - base::Days(10), "c=2"}};
  for (const auto& cookie : cookietest_cases) {
    for (const auto& config : storage_partition_configs) {
      SCOPED_TRACE("partition_name: " + config.partition_name());
      content::StoragePartition* partition =
          profile()->GetStoragePartition(config, false);
      ASSERT_TRUE(partition);
      // Set a partitioned and an unpartitioned cookie for each storage
      // partition. Unpartitioned Cookie
      ASSERT_TRUE(SetCookie(partition, GURL("http://a.com"), cookie.time,
                            cookie.data, absl::nullopt));
      // Partitioned Cookie
      ASSERT_TRUE(SetCookie(
          partition, GURL("https://c.com"), cookie.time,
          cookie.data + "; secure; partitioned",
          net::CookiePartitionKey::FromURLForTesting(GURL("https://d.com"))));
    }
  }

  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    // Each partition should have 4 cookies.
    ASSERT_EQ(GetAllCookies(partition).size(), 4UL);
  }

  ClearTimeRangedData(browsing_data::TimePeriod::LAST_WEEK);
  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    // 2 cookies should be cleared, 2 left.
    EXPECT_EQ(GetAllCookies(partition).size(), 2UL);
  }

  ClearTimeRangedData(browsing_data::TimePeriod::FOUR_WEEKS);
  for (const auto& config : storage_partition_configs) {
    SCOPED_TRACE("partition_name: " + config.partition_name());
    content::StoragePartition* partition =
        profile()->GetStoragePartition(config, false);
    ASSERT_TRUE(partition);
    // All cookies should be cleared, 0 left.
    EXPECT_EQ(GetAllCookies(partition).size(), 0UL);
  }
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowsingDataClearingTest,
                       CrossOriginIframeClearSiteDataHeader) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();
  content::StoragePartition* iwa_main_storage_partition =
      profile()->GetStoragePartition(
          url_info.storage_partition_config(profile()));

  Browser* browser = LaunchWebAppBrowserAndWait(url_info.app_id());
  content::RenderFrameHost* rfh =
      browser->tab_strip_model()->GetActiveWebContents()->GetPrimaryMainFrame();

  ASSERT_NE(https_server()->GetOrigin(), url_info.origin());

  GURL cookie_url =
      https_server()->GetURL("/web_apps/simple_isolated_app/cookie.html");
  CookieSettingsFactory::GetForProfile(browser->profile())
      ->SetCookieSetting(cookie_url, CONTENT_SETTING_ALLOW);
  CreateIframe(rfh, "child_0", cookie_url, "");
  auto* iframe_rfh = content::ChildFrameAt(rfh, 0);

  net::CookieList cookie_list = GetAllCookies(iwa_main_storage_partition);
  ASSERT_EQ(cookie_list.size(), 1UL);
  ASSERT_EQ(cookie_list[0].Domain(), https_server()->GetOrigin().host());

  content::RenderFrameHost* cookie_iframe_rfh = content::ChildFrameAt(rfh, 0);
  SetLocalStorageValue(cookie_iframe_rfh, "foo", "bar");
  ASSERT_EQ(GetLocalStorageCount(cookie_iframe_rfh), 1);
  ASSERT_EQ(GetLocalStorageValue(cookie_iframe_rfh, "foo"), "bar");

  int64_t old_cache_size = GetCacheSize(iwa_main_storage_partition);

  GURL clear_site_data_url = https_server()->GetURL(
      "/web_apps/simple_isolated_app/clear_site_data.html");
  ASSERT_TRUE(
      content::NavigateToURLFromRenderer(iframe_rfh, clear_site_data_url));

  // Not all cache on the StoragePartition is deleted. But it should be smaller
  // than previous value.
  EXPECT_LT(GetCacheSize(iwa_main_storage_partition), old_cache_size);
  // Verify cookie cleared.
  cookie_list = GetAllCookies(iwa_main_storage_partition);
  EXPECT_EQ(cookie_list.size(), 0UL);
  // Verify localStorage cleared.
  EXPECT_EQ(GetLocalStorageCount(content::ChildFrameAt(rfh, 0)), 0);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowsingDataClearingTest,
                       ControlledFrameClearSiteDataHeader) {
  std::vector<const bool> in_memory_test_cases{true, false};
  const std::string partition_name("test_partition");
  for (const bool& in_memory : in_memory_test_cases) {
    SCOPED_TRACE(base::StrCat({"Controlled Frame partition is in-memory: ",
                               (in_memory ? "true" : "false")}));
    IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();

    Browser* browser = LaunchWebAppBrowserAndWait(url_info.app_id());
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();

    ASSERT_NE(https_server()->GetOrigin(), url_info.origin());

    GURL cookie_url =
        https_server()->GetURL("/web_apps/simple_isolated_app/cookie.html");
    ASSERT_TRUE(CreateControlledFrame(
        web_contents, cookie_url,
        base::StrCat({(in_memory ? "" : "persist:") + partition_name})));
    ASSERT_EQ(web_contents->GetInnerWebContents().size(), 1UL);

    content::StoragePartition* controlled_frame_partition =
        profile()->GetStoragePartition(
            url_info.GetStoragePartitionConfigForControlledFrame(
                profile(), partition_name, in_memory),
            /*can_create=*/false);
    ASSERT_TRUE(controlled_frame_partition);

    net::CookieList cookie_list = GetAllCookies(controlled_frame_partition);
    ASSERT_EQ(cookie_list.size(), 1UL);
    ASSERT_EQ(cookie_list[0].Domain(), https_server()->GetOrigin().host());

    content::RenderFrameHost* controlled_frame_rfh =
        web_contents->GetInnerWebContents()[0]->GetPrimaryMainFrame();
    SetLocalStorageValue(controlled_frame_rfh, "foo", "bar");
    ASSERT_EQ(GetLocalStorageCount(controlled_frame_rfh), 1);
    ASSERT_EQ(GetLocalStorageValue(controlled_frame_rfh, "foo"), "bar");

    int64_t old_cache_size = GetCacheSize(controlled_frame_partition);

    GURL clear_site_data_url = https_server()->GetURL(
        "/web_apps/simple_isolated_app/clear_site_data.html");
    ASSERT_TRUE(content::NavigateToURLFromRenderer(controlled_frame_rfh,
                                                   clear_site_data_url));

    // Not all cache on the StoragePartition is deleted. But it should be
    // smaller than previous value.
    EXPECT_LT(GetCacheSize(controlled_frame_partition), old_cache_size);
    // Verify cookie cleared.
    cookie_list = GetAllCookies(controlled_frame_partition);
    EXPECT_EQ(cookie_list.size(), 0UL);
    // Verify localStorage cleared.
    EXPECT_EQ(
        GetLocalStorageCount(
            web_contents->GetInnerWebContents()[0]->GetPrimaryMainFrame()),
        0);
  }
}

}  // namespace web_app
