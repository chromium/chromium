// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/get_isolated_web_app_browsing_data_command.h"

#include "base/containers/flat_map.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using ::testing::Eq;
using ::testing::Ge;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

}  // namespace

class GetIsolatedWebAppBrowsingDataCommandBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  GetIsolatedWebAppBrowsingDataCommandBrowserTest() {
    isolated_web_app_dev_server_ =
        CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  }

  GetIsolatedWebAppBrowsingDataCommandBrowserTest(
      const GetIsolatedWebAppBrowsingDataCommandBrowserTest&) = delete;
  GetIsolatedWebAppBrowsingDataCommandBrowserTest& operator=(
      const GetIsolatedWebAppBrowsingDataCommandBrowserTest&) = delete;

 protected:
  const net::EmbeddedTestServer& isolated_web_app_dev_server() {
    return *isolated_web_app_dev_server_.get();
  }

  WebAppProvider& web_app_provider() {
    auto* web_app_provider = WebAppProvider::GetForTest(profile());
    DCHECK(web_app_provider != nullptr);
    return *web_app_provider;
  }

  void FlushLocalStorage(content::RenderFrameHost* render_frame_host) {
    base::test::TestFuture<void> test_future;
    render_frame_host->GetStoragePartition()->GetLocalStorageControl()->Flush(
        test_future.GetCallback());
    EXPECT_TRUE(test_future.Wait());
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server_;
};

IN_PROC_BROWSER_TEST_F(GetIsolatedWebAppBrowsingDataCommandBrowserTest,
                       NoIsolatedWebAppsInstalled) {
  base::test::TestFuture<base::flat_map<url::Origin, int64_t>> future;
  web_app_provider().scheduler().GetIsolatedWebAppBrowsingData(
      future.GetCallback());
  base::flat_map<url::Origin, int64_t> result = future.Get();

  EXPECT_THAT(result.size(), Eq(0UL));
}

IN_PROC_BROWSER_TEST_F(GetIsolatedWebAppBrowsingDataCommandBrowserTest,
                       IsolatedWebAppWithoutBrowsingData) {
  IsolatedWebAppUrlInfo iwa_url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server().GetOrigin());

  base::test::TestFuture<base::flat_map<url::Origin, int64_t>> future;
  web_app_provider().scheduler().GetIsolatedWebAppBrowsingData(
      future.GetCallback());
  base::flat_map<url::Origin, int64_t> result = future.Get();

  EXPECT_THAT(result, UnorderedElementsAre(Pair(iwa_url_info.origin(), 0)));
}

IN_PROC_BROWSER_TEST_F(GetIsolatedWebAppBrowsingDataCommandBrowserTest,
                       IsolatedWebAppsWithBrowsingData) {
  IsolatedWebAppUrlInfo iwa1_url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server().GetOrigin());
  content::RenderFrameHost* iwa1_frame = OpenApp(iwa1_url_info.app_id());
  EXPECT_TRUE(
      ExecJs(iwa1_frame, "localStorage.setItem('key', '!'.repeat(100))"));
  FlushLocalStorage(iwa1_frame);

  IsolatedWebAppUrlInfo iwa2_url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server().GetOrigin());
  content::RenderFrameHost* iwa2_frame = OpenApp(iwa2_url_info.app_id());
  EXPECT_TRUE(
      ExecJs(iwa2_frame, "localStorage.setItem('key', '!'.repeat(5000))"));
  FlushLocalStorage(iwa2_frame);

  base::test::TestFuture<base::flat_map<url::Origin, int64_t>> future;
  web_app_provider().scheduler().GetIsolatedWebAppBrowsingData(
      future.GetCallback());
  base::flat_map<url::Origin, int64_t> result = future.Get();

  EXPECT_THAT(result,
              UnorderedElementsAre(Pair(iwa1_url_info.origin(), Ge(100)),
                                   Pair(iwa2_url_info.origin(), Ge(5000))));
}

}  // namespace web_app
