// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/iwa_test_server_configurator.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/liburlpattern/part.h"

namespace web_app {

using testing::ElementsAre;
using testing::IsEmpty;

namespace {

// The update channel used in tests. Could be any channel.
UpdateChannel StableChannel() {
  return UpdateChannel::Create("stable").value();
}

web_package::SignedWebBundleId TestIwaWebBundleId() {
  return test::GetDefaultEd25519WebBundleId();
}

webapps::AppId GetAppId(web_package::SignedWebBundleId web_bundle_id) {
  return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id)
      .app_id();
}

const WebApp& GetAppById(FakeWebAppProvider& provider,
                         const web_package::SignedWebBundleId& bundle_id) {
  return CHECK_DEREF(
      provider.registrar_unsafe().GetAppById(GetAppId(bundle_id)));
}

std::unique_ptr<ScopedBundledIsolatedWebApp> CreateBundle(
    Profile& profile,
    ManifestBuilder& manifest) {
  std::unique_ptr<ScopedBundledIsolatedWebApp> app =
      IsolatedWebAppBuilder(manifest).BuildBundle(
          test::GetDefaultEd25519KeyPair());
  app->TrustSigningKey();
  app->FakeInstallPageState(&profile);
  return app;
}

// Matches a web app that has the given `version`.
testing::Matcher<const WebApp&> HasVersion(std::string_view version) {
  using testing::Optional;
  using testing::Property;
  auto iwa_version = IwaVersion::Create(version).value();
  // Equivalent to `app->isolation_data()->version() == iwa_version`.
  return Property(
      "isolation_data", &WebApp::isolation_data,
      Optional(Property("version", &IsolationData::version, iwa_version)));
}

// Returns a pattern to match the "/foo" pathname. Could be any pattern.
blink::SafeUrlPattern FooPattern() {
  blink::SafeUrlPattern pattern;
  pattern.pathname = {liburlpattern::Part(liburlpattern::PartType::kFixed,
                                          /*value=*/"/foo",
                                          liburlpattern::Modifier::kNone)};
  return pattern;
}

}  // namespace

// Verifies manifest changes across IWA updates propagate correctly to `WebApp`.
class ManifestUpdateTest : public IsolatedWebAppTest {
 public:
  ManifestUpdateTest()
      : IsolatedWebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  }
  ~ManifestUpdateTest() override = default;

 protected:
  void SetUp() override {
    IsolatedWebAppTest::SetUp();
    provider().SetEnableAutomaticIwaUpdates(
        FakeWebAppProvider::AutomaticIwaUpdateStrategy::kForceEnabled);

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  // The test IWA used in tests. Must be called after `InstallInitialApp`.
  const WebApp& TestIwa() {
    return GetAppById(provider(), TestIwaWebBundleId());
  }

  // Creates version 1.0.0 of a fake test app with the given `manifest`, serves
  // it in the test server, configures policy to force install it, and waits
  // until it is installed.
  void InstallInitialTestIwa(ManifestBuilder manifest = ManifestBuilder()) {
    test_update_server().AddBundle(
        CreateBundle(CHECK_DEREF(profile()), manifest.SetVersion("1.0.0")),
        {{StableChannel()}});

    test::AddForceInstalledIwaToPolicy(
        profile()->GetPrefs(),
        test_update_server().CreateForceInstallPolicyEntry(
            TestIwaWebBundleId(),
            /*update_channel=*/StableChannel()));

    web_app::WebAppTestInstallObserver(profile()).BeginListeningAndWait(
        {GetAppId(TestIwaWebBundleId())});

    ASSERT_THAT(TestIwa(), HasVersion("1.0.0"));
  }

  // Updates the fake test app in the test server to version 2.0.0 with the
  // given `manifest`, and waits until the update is installed.
  //
  // Must be called after `InstallInitialApp`.
  void UpdateTestIwa(ManifestBuilder manifest = ManifestBuilder()) {
    EXPECT_THAT(TestIwa(), HasVersion("1.0.0"));

    test_update_server().AddBundle(
        CreateBundle(CHECK_DEREF(profile()), manifest.SetVersion("2.0.0")),
        {{StableChannel()}});

    base::TimeTicks update_time = provider()
                                      .iwa_update_manager()
                                      .GetNextUpdateDiscoveryTimeForTesting()
                                      .value();
    task_environment().FastForwardBy(update_time - base::TimeTicks::Now());

    ASSERT_THAT(TestIwa(), HasVersion("2.0.0"));
  }
};

TEST_F(ManifestUpdateTest, BorderlessUrlPatterns) {
  InstallInitialTestIwa();
  EXPECT_THAT(TestIwa().borderless_url_patterns(), IsEmpty());

  UpdateTestIwa(ManifestBuilder().AddBorderlessUrlPattern(FooPattern()));
  EXPECT_THAT(TestIwa().borderless_url_patterns(), ElementsAre(FooPattern()));
}

}  // namespace web_app
