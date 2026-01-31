// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_user_installed_manager.h"

#include <memory>

#include "base/functional/bind_internal.h"
#include "base/test/gmock_expected_support.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_iwa_runtime_data_provider_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class IsolatedWebAppUserInstalledManagerTest : public IsolatedWebAppTest {
 public:
  void SetUpServedIwas() {
    std::unique_ptr<ScopedBundledIsolatedWebApp> app1 =
        IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(test::GetDefaultEd25519KeyPair());
    app1->FakeInstallPageState(profile());

    test_update_server().AddBundle(std::move(app1));
  }

  void SetUp() override {
    IsolatedWebAppTest::SetUp();

    test::AwaitStartWebAppProviderAndSubsystems(profile());
    SetUpServedIwas();
  }

  void AssertAppInstalled(const web_package::SignedWebBundleId& swbn_id) {
    const WebApp* web_app = provider().registrar_unsafe().GetAppById(
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(swbn_id).app_id());
    ASSERT_THAT(web_app, testing::NotNull()) << "The app in not installed :(";
  }
};

TEST_F(IsolatedWebAppUserInstalledManagerTest, AppRemovedAfterBlocklisting) {
  const std::unique_ptr<ScopedBundledIsolatedWebApp> bundle =
      IsolatedWebAppBuilder(ManifestBuilder().SetVersion("1.0.0"))
          .BuildBundle(test::GetDefaultEd25519KeyPair());

  IsolatedWebAppUrlInfo url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          bundle->web_bundle_id());
  {
    WebAppTestInstallObserver install_observer(profile());
    install_observer.BeginListening();
    EXPECT_EQ(
        url_info,
        bundle->InstallWithSource(
            profile(), &IsolatedWebAppInstallSource::FromGraphicalInstaller));

    EXPECT_EQ(install_observer.Wait(), url_info.app_id());
  }
  AssertAppInstalled(url_info.web_bundle_id());

  {
    WebAppTestUninstallObserver uninstall_observer(profile());
    uninstall_observer.BeginListening();

    EXPECT_THAT(test::KeyDistributionComponentBuilder(base::Version("1.0.0"))
                    .AddToBlocklist(url_info.web_bundle_id())
                    .Build()
                    .UploadFromComponentFolder(),
                base::test::HasValue());

    EXPECT_EQ(uninstall_observer.Wait(), url_info.app_id());
  }

  EXPECT_THAT(provider().registrar_unsafe().GetAppById(url_info.app_id()),
              testing::IsNull());
}

}  // namespace web_app
