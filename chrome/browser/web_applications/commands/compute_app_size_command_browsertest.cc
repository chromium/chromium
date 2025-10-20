// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/compute_app_size_command.h"

#include <algorithm>
#include <utility>

#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const web_package::test::Ed25519KeyPair kPublicKeyPair1 =
    web_package::test::Ed25519KeyPair::CreateRandom();

const web_package::SignedWebBundleId kWebBundleId1 =
    web_package::SignedWebBundleId::CreateForPublicKey(
        kPublicKeyPair1.public_key);
}  // namespace

namespace web_app {

namespace {
bool CheckAppSizesNotNull(WebAppProvider& provider,
                          const webapps::AppId& app_id) {
  // We need to wait for the quota manager to receive storage data from the
  // renderer process. As updates to quota manager usage occurs on a different
  // sequence to this process, it requires multiple events. Due to all of this,
  // we are resorting to polling for non-zero values.
  while (true) {
    base::test::TestFuture<std::optional<ComputedAppSizeWithOrigin>> app_size;
    provider.scheduler().ComputeAppSize(app_id, app_size.GetCallback());
    auto proxy = std::move(app_size.Get());

    if (proxy->app_size_in_bytes() > 0u && proxy->data_size_in_bytes() > 0u) {
      return true;
    }
  }
}
}  // namespace

class ComputeAppSizeCommandForWebAppBrowserTest : public WebAppBrowserTestBase {
 private:
  base::test::ScopedFeatureList feature_list_{features::kWebAppUsePrimaryIcon};
};

IN_PROC_BROWSER_TEST_F(ComputeAppSizeCommandForWebAppBrowserTest,
                       RetrieveWebAppSize) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL app_url = embedded_test_server()->GetURL("/web_apps/basic.html");
  webapps::AppId app_id = InstallWebAppFromPage(browser(), app_url);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  const char* script = R"(
        localStorage.setItem('data', 'data'.repeat(5000));
        location.href = 'about:blank';
        true;
      )";

  EXPECT_TRUE(
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), script)
          .ExtractBool());
  ASSERT_TRUE(CheckAppSizesNotNull(provider(), app_id));
}

class ComputeAppSizeCommandForIsolatedWebAppBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  ComputeAppSizeCommandForIsolatedWebAppBrowserTest() = default;

  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    IwaKeyDistributionInfoProvider::GetInstance()
        .SkipManagedAllowlistChecksForTesting(true);
    iwa_test_update_server_.AddBundle(
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(kPublicKeyPair1));
  }

  void TearDownOnMainThread() override {
    IwaKeyDistributionInfoProvider::GetInstance()
        .SkipManagedAllowlistChecksForTesting(false);
    IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  ComputeAppSizeCommandForIsolatedWebAppBrowserTest(
      const ComputeAppSizeCommandForIsolatedWebAppBrowserTest&) = delete;
  ComputeAppSizeCommandForIsolatedWebAppBrowserTest& operator=(
      const ComputeAppSizeCommandForIsolatedWebAppBrowserTest&) = delete;

 protected:
  void SetIwaForceInstallPolicy(base::Value::List update_manifest_entries) {
    profile()->GetPrefs()->SetList(prefs::kIsolatedWebAppInstallForceList,
                                   std::move(update_manifest_entries));
  }

  IsolatedWebAppTestUpdateServer iwa_test_update_server_;

#if !BUILDFLAG(IS_CHROMEOS)
 private:
  base::test::ScopedFeatureList feature_list_{features::kWebAppUsePrimaryIcon};
#endif  // !BUILDFLAG(IS_CHROMEOS)
};

IN_PROC_BROWSER_TEST_F(ComputeAppSizeCommandForIsolatedWebAppBrowserTest,
                       RetrieveWebAppSize) {
  const webapps::AppId app_id =
      web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(kWebBundleId1)
          .app_id();

  WebAppTestInstallObserver install_observer(profile());
  SetIwaForceInstallPolicy(base::Value::List().Append(
      iwa_test_update_server_.CreateForceInstallPolicyEntry(kWebBundleId1)));
  ASSERT_EQ(install_observer.BeginListeningAndWait({app_id}), app_id);

  auto* browser = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  ASSERT_TRUE(browser);

  const char* script = R"(
        localStorage.setItem('data', 'data'.repeat(5000));
        location.href = 'about:blank';
        true;
      )";

  EXPECT_TRUE(EvalJs(browser->tab_strip_model()->GetActiveWebContents(), script)
                  .ExtractBool());

  ASSERT_TRUE(CheckAppSizesNotNull(provider(), app_id));
}

}  // namespace web_app
