// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"

#include "base/test/gmock_expected_support.h"
#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "components/nacl/browser/nacl_browser.h"
#endif  // BUILDFLAG(ENABLE_NACL)

namespace web_app {

IsolatedWebAppTest::~IsolatedWebAppTest() = default;

TestingProfile* IsolatedWebAppTest::profile() {
  return profile_.get();
}
FakeWebAppProvider& IsolatedWebAppTest::provider() {
  return *FakeWebAppProvider::Get(profile());
}
content::BrowserTaskEnvironment& IsolatedWebAppTest::task_environment() {
  return *env_;
}
network::TestURLLoaderFactory& IsolatedWebAppTest::url_loader_factory() {
  return url_loader_factory_;
}
IwaTestServerConfigurator& IsolatedWebAppTest::test_update_server() {
  return test_update_server_;
}

void IsolatedWebAppTest::SetUp() {
  ASSERT_TRUE(profile_manager_.SetUp());
  profile_ = profile_manager_.CreateTestingProfile(
      TestingProfile::kDefaultProfileUserName, /*is_main_profile=*/true,
      url_loader_factory_.GetSafeWeakWrapper());

#if BUILDFLAG(ENABLE_NACL)
  // Clearing Cache will clear PNACL cache, which needs this delegate set.
  nacl::NaClBrowser::SetDelegate(std::make_unique<NaClBrowserDelegateImpl>(
      profile_manager_.profile_manager()));
#endif  // BUILDFLAG(ENABLE_NACL)

  if (kdc_type_ == KeyDistributionComponentType::kDownloaded) {
    EXPECT_THAT(test::UpdateKeyDistributionInfo(base::Version("0.0.1"),
                                                IwaKeyDistribution()),
                base::test::HasValue());
  }
}

void IsolatedWebAppTest::TearDown() {
#if BUILDFLAG(ENABLE_NACL)
  nacl::NaClBrowser::ClearAndDeleteDelegate();
#endif  // BUILDFLAG(ENABLE_NACL)

  // Manually shut down the provider and subsystems so that async tasks are
  // stopped.
  // Note: `DeleteAllTestingProfiles` doesn't actually destruct profiles and
  // therefore doesn't Shutdown keyed services like the provider.
  provider().Shutdown();
  os_integration_test_override_.reset();

  profile_ = nullptr;
  profile_manager_.DeleteAllTestingProfiles();

  IwaKeyDistributionInfoProvider::GetInstance()->DestroyInstanceForTesting();

  env_.reset();
}

IsolatedWebAppTest::IsolatedWebAppTest(
    std::unique_ptr<content::BrowserTaskEnvironment> env,
    bool dev_mode,
    KeyDistributionComponentType kdc_type)
    : env_(std::move(env)), kdc_type_(kdc_type) {
  std::vector<base::test::FeatureRef> enabled_features = {
      features::kIsolatedWebApps,
      component_updater::kIwaKeyDistributionComponent};
  if (dev_mode) {
    enabled_features.push_back(features::kIsolatedWebAppDevMode);
  }
  features_.InitWithFeatures(enabled_features, {});
}

}  // namespace web_app
