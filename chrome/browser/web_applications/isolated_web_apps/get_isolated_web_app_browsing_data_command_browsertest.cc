// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/get_isolated_web_app_browsing_data_command.h"

#include <memory>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
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
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Lt;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

}  // namespace

class GetIsolatedWebAppBrowsingDataCommandBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  GetIsolatedWebAppBrowsingDataCommandBrowserTest()
      : app_(IsolatedWebAppBuilder(
                 ManifestBuilder().AddPermissionsPolicy(
                     blink::mojom::PermissionsPolicyFeature::kControlledFrame,
                     /*self=*/true,
                     /*origins=*/{}))
                 .BuildAndStartProxyServer()) {}

  GetIsolatedWebAppBrowsingDataCommandBrowserTest(
      const GetIsolatedWebAppBrowsingDataCommandBrowserTest&) = delete;
  GetIsolatedWebAppBrowsingDataCommandBrowserTest& operator=(
      const GetIsolatedWebAppBrowsingDataCommandBrowserTest&) = delete;

 protected:
  IsolatedWebAppUrlInfo InstallApp() { return app_->InstallChecked(profile()); }

  GURL proxy_server_url() { return app_->proxy_server().base_url(); }

  WebAppProvider& web_app_provider() {
    auto* web_app_provider = WebAppProvider::GetForTest(profile());
    DCHECK(web_app_provider != nullptr);
    return *web_app_provider;
  }

  void CreateControlledFrame(content::RenderFrameHost* iwa_frame,
                             GURL url,
                             std::string_view partition,
                             int bytes) {
    EXPECT_TRUE(ExecJs(iwa_frame, content::JsReplace(R"(
        (async function() {
          const controlledframe = document.createElement('controlledframe');
          controlledframe.setAttribute('src', $1);
          controlledframe.setAttribute('partition', $2);
          await new Promise((resolve, reject) => {
            controlledframe.addEventListener('loadcommit', resolve);
            controlledframe.addEventListener('loadabort', reject);
            document.body.appendChild(controlledframe);
          });
          return await controlledframe.executeScript(
              {code: 'localStorage.setItem("test", "!".repeat($3))'});
        })();
      )",
                                                     url, partition, bytes)));
  }

 private:
  std::unique_ptr<ScopedProxyIsolatedWebApp> app_;
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kControlledFrame};
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
  IsolatedWebAppUrlInfo iwa_url_info = InstallApp();

  base::test::TestFuture<base::flat_map<url::Origin, int64_t>> future;
  web_app_provider().scheduler().GetIsolatedWebAppBrowsingData(
      future.GetCallback());
  base::flat_map<url::Origin, int64_t> result = future.Get();

  EXPECT_THAT(result, UnorderedElementsAre(Pair(iwa_url_info.origin(), 0)));
}

IN_PROC_BROWSER_TEST_F(GetIsolatedWebAppBrowsingDataCommandBrowserTest,
                       IsolatedWebAppsWithBrowsingData) {
  IsolatedWebAppUrlInfo iwa1_url_info = InstallApp();
  content::RenderFrameHost* iwa1_frame = OpenApp(iwa1_url_info.app_id());
  EXPECT_TRUE(
      ExecJs(iwa1_frame, "localStorage.setItem('key', '!'.repeat(100))"));

  IsolatedWebAppUrlInfo iwa2_url_info = InstallApp();
  content::RenderFrameHost* iwa2_frame = OpenApp(iwa2_url_info.app_id());
  EXPECT_TRUE(
      ExecJs(iwa2_frame, "localStorage.setItem('key', '!'.repeat(5000))"));

  base::test::TestFuture<base::flat_map<url::Origin, int64_t>> future;
  web_app_provider().scheduler().GetIsolatedWebAppBrowsingData(
      future.GetCallback());
  base::flat_map<url::Origin, int64_t> result = future.Get();

  EXPECT_THAT(result,
              UnorderedElementsAre(Pair(iwa1_url_info.origin(), Ge(100)),
                                   Pair(iwa2_url_info.origin(), Ge(5000))));
}

IN_PROC_BROWSER_TEST_F(GetIsolatedWebAppBrowsingDataCommandBrowserTest,
                       IsolatedWebAppWithControlledFrameData) {
  IsolatedWebAppUrlInfo iwa_url_info = InstallApp();
  content::RenderFrameHost* iwa_frame = OpenApp(iwa_url_info.app_id());
  CreateControlledFrame(iwa_frame, proxy_server_url(), "persist:a", 2000);
  CreateControlledFrame(iwa_frame, proxy_server_url(), "in_memory", 1000);

  base::test::TestFuture<base::flat_map<url::Origin, int64_t>> future;
  web_app_provider().scheduler().GetIsolatedWebAppBrowsingData(
      future.GetCallback());
  base::flat_map<url::Origin, int64_t> result = future.Get();

  EXPECT_THAT(result, UnorderedElementsAre(Pair(iwa_url_info.origin(),
                                                AllOf(Ge(2000), Lt(2100)))));
}

}  // namespace web_app
