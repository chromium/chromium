// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test.h"

#include "base/test/test_timeouts.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/features.h"
#include "chrome/common/chrome_features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "content/public/common/content_features.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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
      TestingProfile::kDefaultProfileUserName, /*testing_factories=*/{},
      url_loader_factory_.GetSafeWeakWrapper());
}

void IsolatedWebAppTest::TearDown() {
  task_environment().RunUntilIdle();
  // Manually shut down the provider and subsystems so that async tasks are
  // stopped.
  // Note: `DeleteAllTestingProfiles` doesn't actually destruct profiles and
  // therefore doesn't Shutdown keyed services like the provider.
  provider().Shutdown();

  ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(profile())
      ->Shutdown();

  if (task_environment().UsesMockTime()) {
    // TODO(crbug.com/299074540): Without this line, subsequent tests are unable
    // to use `test::UninstallWebApp`, which will hang forever. This has
    // something to do with the combination of `MOCK_TIME` and NaCl, because
    // the code ends up hanging forever in
    // `PnaclTranslationCache::DoomEntriesBetween`. A simple `FastForwardBy`
    // here seems to alleviate this issue.
    task_environment().FastForwardBy(TestTimeouts::tiny_timeout());
  }

  os_integration_test_override_.reset();

  profile_ = nullptr;
  profile_manager_.DeleteAllTestingProfiles();

  env_.reset();
}

IsolatedWebAppTest::IsolatedWebAppTest(
    std::unique_ptr<content::BrowserTaskEnvironment> env,
    bool dev_mode)
    : env_(std::move(env)) {
  std::vector<base::test::FeatureRef> enabled_features = {
      features::kIsolatedWebAppManagedAllowlist,
#if !BUILDFLAG(IS_CHROMEOS)
      features::kIsolatedWebApps,
#endif  // !BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      component_updater::kIwaKeyDistributionComponent
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  };
  if (dev_mode) {
    enabled_features.push_back(features::kIsolatedWebAppDevMode);
  }
  features_.InitWithFeatures(enabled_features, {});
}

}  // namespace web_app
