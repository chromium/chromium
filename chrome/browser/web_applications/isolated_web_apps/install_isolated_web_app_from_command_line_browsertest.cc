// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_builder.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
    : public WebAppControllerBrowserTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
    WebAppControllerBrowserTest::SetUp();
  }

  WebAppRegistrar& GetWebAppRegistrar() {
    auto* provider = WebAppProvider::GetForTest(browser()->profile());
    CHECK(provider != nullptr);
    return provider->registrar_unsafe();
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
    CHECK(command_line != nullptr);

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
  webapps::AppId id = observer.BeginListeningAndWait();

  EXPECT_THAT(GetWebAppRegistrar().IsInstalled(id), IsTrue());
  EXPECT_THAT(GetWebAppRegistrar().GetAppById(id),
              test::IwaIs(Eq("Simple Isolated App"),
                          test::IsolationDataIs(
                              VariantWith<DevModeProxy>(
                                  Field(&DevModeProxy::proxy_url,
                                        Eq(url::Origin::Create(GetAppUrl())))),
                              Eq(base::Version("1.0.0")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(absl::nullopt))));
}

class InstallIsolatedWebAppFromCommandLineFromFileBrowserTest
    : public InstallIsolatedWebAppFromCommandLineBrowserTest {
 protected:
  void SetUp() override {
    CHECK(scoped_temp_dir_.CreateUniqueTempDir());
    // Add a "foo" subdirectory so that we can add a relative segment to the Web
    // Bundle path to test that it is converted into an absolute path when
    // persisted to the Web App Database.
    CHECK(base::CreateDirectory(scoped_temp_dir_.GetPath().AppendASCII("foo")));
    signed_web_bundle_path_ =
        scoped_temp_dir_.GetPath()
            .AppendASCII("foo")
            .Append(base::FilePath::kParentDirectory)
            .Append(base::FilePath::FromASCII("test-bundle.swbn"));
    TestSignedWebBundle bundle = TestSignedWebBundleBuilder::BuildDefault();
    bundle_id_ = std::make_unique<web_package::SignedWebBundleId>(bundle.id);
    CHECK(base::WriteFile(signed_web_bundle_path_, bundle.data));

    InstallIsolatedWebAppFromCommandLineBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CHECK(command_line != nullptr);
    command_line->AppendSwitchPath("install-isolated-web-app-from-file",
                                   signed_web_bundle_path_);
    InstallIsolatedWebAppFromCommandLineBrowserTest::SetUpCommandLine(
        command_line);
  }

  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath signed_web_bundle_path_;
  std::unique_ptr<web_package::SignedWebBundleId> bundle_id_;
};

IN_PROC_BROWSER_TEST_F(InstallIsolatedWebAppFromCommandLineFromFileBrowserTest,
                       AppFromCommandLineIsInstalled) {
  WebAppTestInstallObserver observer(browser()->profile());
  webapps::AppId id = observer.BeginListeningAndWait();

  ASSERT_TRUE(bundle_id_);
  ASSERT_EQ(
      id,
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(*bundle_id_).app_id());
  ASSERT_THAT(GetWebAppRegistrar().IsInstalled(id), IsTrue());

  // Even if we didn't artificially add a relative `foo/..` segment to
  // `signed_web_bundle_path_`, it might still not be absolute _and_ free of
  // symlinks, because there is no guarantee that `ScopedTempDir::GetPath`
  // returns a symlink-free path (this was observed on macOS 14,
  // crbug.com/1454276). `base::MakeAbsoluteFilePath`, which the code under test
  // uses as well, will resolve those symlinks, hence we also need to do so
  // here.
  base::FilePath absolute_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    absolute_path = base::MakeAbsoluteFilePath(signed_web_bundle_path_);
  }

  EXPECT_THAT(GetWebAppRegistrar().GetAppById(id),
              test::IwaIs(Eq("Simple Isolated App"),
                          test::IsolationDataIs(
                              VariantWith<DevModeBundle>(Field(
                                  &DevModeBundle::path, Eq(absolute_path))),
                              Eq(base::Version("1.0.0")),
                              /*controlled_frame_partitions=*/_,
                              /*pending_update_info=*/Eq(absl::nullopt))));
}

}  // namespace
}  // namespace web_app
