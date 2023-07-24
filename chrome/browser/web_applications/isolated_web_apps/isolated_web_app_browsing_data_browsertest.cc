// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
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

// Evaluates to true if the test value is within 5% of the given value.
MATCHER_P(IsApproximately, approximate_value, "") {
  return arg > (approximate_value * 0.95) && arg < (approximate_value * 1.05);
}

class IsolatedWebAppBrowsingDataTest : public IsolatedWebAppBrowserTestHarness {
 protected:
  IsolatedWebAppBrowsingDataTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIwaControlledFrame);
  }

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
  base::test::ScopedFeatureList scoped_feature_list_;
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

    auto filter = content::BrowsingDataFilterBuilder::Create(
        content::BrowsingDataFilterBuilder::Mode::kDelete);
    filter->AddOrigin(url_info.origin());

    browsing_data_remover->RemoveWithFilter(
        /*delete_begin=*/base::Time(), /*delete_end=*/base::Time::Max(),
        chrome_browsing_data_remover::DATA_TYPE_SITE_DATA &
            ~content::BrowsingDataRemover::DATA_TYPE_COOKIES,
        chrome_browsing_data_remover::ALL_ORIGIN_TYPES, std::move(filter));
    run_loop.Run();

    browsing_data_remover->SetWouldCompleteCallbackForTesting(
        base::DoNothing());
  }

  int64_t GetCacheSize(const IsolatedWebAppUrlInfo& url_info) {
    base::test::TestFuture<bool, int64_t> future;

    content::StoragePartition* storage_partition =
        profile()->GetStoragePartition(
            url_info.storage_partition_config(profile()));

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
      const IsolatedWebAppUrlInfo& url_info,
      const GURL& url,
      const std::string& cookie_line,
      const absl::optional<net::CookiePartitionKey>& cookie_partition_key) {
    content::StoragePartition* storage_partition =
        profile()->GetStoragePartition(
            url_info.storage_partition_config(profile()));

    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    storage_partition->GetNetworkContext()->GetCookieManager(
        cookie_manager.BindNewPipeAndPassReceiver());

    auto cookie_obj = net::CanonicalCookie::Create(
        url, cookie_line, base::Time::Now(), /*server_time=*/absl::nullopt,
        cookie_partition_key);

    base::test::TestFuture<net::CookieAccessResult> future;
    cookie_manager->SetCanonicalCookie(*cookie_obj, url,
                                       net::CookieOptions::MakeAllInclusive(),
                                       future.GetCallback());
    return future.Take().status.IsInclude();
  }

  net::CookieList GetAllCookies(const IsolatedWebAppUrlInfo& url_info) {
    content::StoragePartition* storage_partition =
        profile()->GetStoragePartition(
            url_info.storage_partition_config(profile()));

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
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();

  // IWA installation creates cache data.
  EXPECT_GT(GetCacheSize(url_info), 0);

  // TODO(crbug.com/1453520): Clear cache data.
  // EXPECT_EQ(GetCacheSize(url_info), 0);
}

IN_PROC_BROWSER_TEST_F(IsolatedWebAppBrowsingDataClearingTest, CookieCleared) {
  IsolatedWebAppUrlInfo url_info = InstallIsolatedWebApp();

  // Unpartitioned Cookie
  ASSERT_TRUE(SetCookie(url_info, GURL("http://a.com"), "A=0", absl::nullopt));

  // Partitioned Cookie
  ASSERT_TRUE(SetCookie(
      url_info, GURL("https://c.com"), "A=0; secure; partitioned",
      net::CookiePartitionKey::FromURLForTesting(GURL("https://d.com"))));

  EXPECT_EQ(GetAllCookies(url_info).size(), 2UL);

  // TODO(crbug.com/1453520): Clear cookies.
  // EXPECT_GT(GetAllCookies(url_info).size(), 0UL);
}

}  // namespace web_app
