// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_from_command_line.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/one_shot_event.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace web_app {
namespace {
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsTrue;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::VariantWith;

class InstallIsolatedWebAppFromCommandLineBrowserTest
    : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
    InProcessBrowserTest::SetUp();
  }

  WebAppRegistrar& GetWebAppRegistrar() {
    auto* provider = WebAppProvider::GetForTest(browser()->profile());
    DCHECK(provider != nullptr);
    return provider->registrar();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class InstallIsolatedWebAppFromCommandLineFromUrlBrowserTest
    : public InstallIsolatedWebAppFromCommandLineBrowserTest {
 protected:
  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(
        GetChromeTestDataDir().AppendASCII("web_apps/simple_isolated_app"));
    ASSERT_TRUE(embedded_test_server()->Start());

    InstallIsolatedWebAppFromCommandLineBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DCHECK(command_line != nullptr);

    command_line->AppendSwitchASCII("install-isolated-web-app-from-url",
                                    GetAppUrl().spec());
    InstallIsolatedWebAppFromCommandLineBrowserTest::SetUpCommandLine(
        command_line);
  }

  GURL GetAppUrl() const { return embedded_test_server()->base_url(); }
};

IN_PROC_BROWSER_TEST_F(InstallIsolatedWebAppFromCommandLineFromUrlBrowserTest,
                       AppFromCommandLineIsInstalled) {
  WebAppTestInstallObserver observer(browser()->profile());
  AppId id = observer.BeginListeningAndWait();

  ASSERT_THAT(GetWebAppRegistrar().IsInstalled(id), IsTrue());

  EXPECT_THAT(
      GetWebAppRegistrar().GetAppById(id),
      Pointee(Property(
          &WebApp::isolation_data,
          Optional(Field(&IsolationData::content,
                         VariantWith<IsolationData::DevModeProxy>(
                             Field(&IsolationData::DevModeProxy::proxy_url,
                                   Eq(url::Origin::Create(GetAppUrl())))))))));
}

class InstallIsolatedWebAppFromCommandLineFromFileBrowserTest
    : public InstallIsolatedWebAppFromCommandLineBrowserTest {
 protected:
  void SetUp() override {
    base::ScopedAllowBlockingForTesting allow_blocking;

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    signed_web_bundle_path_ =
        scoped_temp_dir_.GetPath().AppendASCII("test_bundle.swbn");
    CreateSignedWebBundle(signed_web_bundle_path_);

    InstallIsolatedWebAppFromCommandLineBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DCHECK(command_line != nullptr);
    command_line->AppendSwitchPath("install-isolated-web-app-from-file",
                                   signed_web_bundle_path_);
    InstallIsolatedWebAppFromCommandLineBrowserTest::SetUpCommandLine(
        command_line);
  }

  void CreateSignedWebBundle(base::FilePath path) {
    std::vector<uint8_t> bundle = BuildBundle();
    DCHECK(base::WriteFile(path, bundle));
  }

  std::vector<uint8_t> BuildBundle() {
    web_package::WebBundleSigner::KeyPair key_pair =
        web_package::WebBundleSigner::KeyPair::CreateRandom();
    web_package::SignedWebBundleId bundle_id =
        web_package::SignedWebBundleId::CreateForEd25519PublicKey(
            key_pair.public_key);
    web_package::WebBundleBuilder builder;

    builder.AddPrimaryURL("isolated-app://" + bundle_id.id());
    builder.AddExchange("isolated-app://" + bundle_id.id(),
                        {{":status", "200"}, {"content-type", "text/plain"}},
                        "payload");

    auto unsigned_bundle = builder.CreateBundle();
    return web_package::WebBundleSigner::SignBundle(unsigned_bundle,
                                                    {key_pair});
  }

  base::FilePath WebBundlePath() const { return signed_web_bundle_path_; }

 private:
  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath signed_web_bundle_path_;
};

// TODO: http://b/232991707 Enable this test with dev-mode signed web bundle
// implementation.
IN_PROC_BROWSER_TEST_F(InstallIsolatedWebAppFromCommandLineFromFileBrowserTest,
                       DISABLED_AppFromCommandLineIsInstalled) {
  WebAppTestInstallObserver observer(browser()->profile());
  AppId id = observer.BeginListeningAndWait();

  ASSERT_THAT(GetWebAppRegistrar().IsInstalled(id), IsTrue());

  EXPECT_THAT(GetWebAppRegistrar().GetAppById(id),
              Pointee(Property(
                  &WebApp::isolation_data,
                  Optional(Field(&IsolationData::content,
                                 VariantWith<IsolationData::DevModeBundle>(
                                     Field(&IsolationData::DevModeBundle::path,
                                           Eq(WebBundlePath()))))))));
}

}  // namespace
}  // namespace web_app
